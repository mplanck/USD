//
// Copyright 2016 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#include "usdMaya/usdWriteJobCtx.h"

#include "pxr/usd/ar/resolver.h"
#include "pxr/usd/ar/resolverContext.h"
#include "pxr/usd/usdGeom/scope.h"

#include "usdMaya/MayaCameraWriter.h"
#include "usdMaya/MayaMeshWriter.h"
#include "usdMaya/MayaNurbsCurveWriter.h"
#include "usdMaya/MayaNurbsSurfaceWriter.h"
#include "usdMaya/MayaTransformWriter.h"
#include "usdMaya/primWriterRegistry.h"

#include <maya/MDagPathArray.h>
#include <maya/MGlobal.h>
#include <maya/MString.h>
#include <maya/MPxNode.h>

#include <sstream>

PXR_NAMESPACE_OPEN_SCOPE

namespace {
    inline
    SdfPath& rootOverridePath(const JobExportArgs& args, SdfPath& path) {
        if (!args.usdModelRootOverridePath.IsEmpty() ) {
            path = path.ReplacePrefix(path.GetPrefixes()[0], args.usdModelRootOverridePath);
        }
        return path;
    }

    constexpr auto instancesScopeName = "/InstanceSources";
}

usdWriteJobCtx::usdWriteJobCtx(const JobExportArgs& args) : mArgs(args), mNoInstances(true)
{

}

SdfPath usdWriteJobCtx::getMasterPath(const MDagPath& dg)
{
    const MObjectHandle handle(dg.node());
    const auto it = mMasterToUsdPath.find(handle);
    if (it != mMasterToUsdPath.end()) {
        return it->second;
    } else {
        MDagPathArray allInstances;
        auto status = MDagPath::getAllPathsTo(dg.node(), allInstances);
        if (!status || (allInstances.length() == 0)) { return SdfPath(); }
        // we are looking for the instance with the lowest number here
        // which is still exported
        auto dagCopy = allInstances[0];
        const auto usdPath = getUsdPathFromDagPath(dagCopy, true);
        dagCopy.pop();
        // This will get auto destroyed, because we are not storing it in the list
        MayaTransformWriterPtr transformPrimWriter(new MayaTransformWriter(dagCopy, usdPath.GetParentPath(), true, *this));
        if (transformPrimWriter != nullptr && transformPrimWriter->isValid()) {
            transformPrimWriter->write(UsdTimeCode::Default());
            mMasterToUsdPath.insert(std::make_pair(handle, transformPrimWriter->getUsdPath()));
        } else {
            return SdfPath();
        }
        auto primWriter = _createPrimWriter(allInstances[0], true);
        if (primWriter != nullptr) {
            primWriter->write(UsdTimeCode::Default());
            mMayaPrimWriterList.push_back(primWriter);
            return transformPrimWriter->getUsdPath();
        } else {
            return SdfPath();
        }
    }
}

bool usdWriteJobCtx::needToTraverse(const MDagPath& curDag)
{
    MObject ob = curDag.node();
    // NOTE: Already skipping all intermediate objects
    // skip all intermediate nodes (and their children)
    if (PxrUsdMayaUtil::isIntermediate(ob)) {
        return false;
    }

    // skip nodes that aren't renderable (and their children)

    if (mArgs.excludeInvisible && !PxrUsdMayaUtil::isRenderable(ob)) {
        return false;
    }

    if (!mArgs.exportDefaultCameras && ob.hasFn(MFn::kTransform) && curDag.length() == 1) {
        // Ignore transforms of default cameras
        MString fullPathName = curDag.fullPathName();
        if (fullPathName == "|persp" ||
            fullPathName == "|top" ||
            fullPathName == "|front" ||
            fullPathName == "|side") {
            return false;
        }
    }

    return true;
}

SdfPath usdWriteJobCtx::getUsdPathFromDagPath(const MDagPath& dagPath, bool instanceSource /* = false */)
{
    SdfPath path;
    if (instanceSource) {
        if (mInstancesPrim) {
            mNoInstances = false;
            std::stringstream ss;
            ss << mInstancesPrim.GetPath().GetString();
            MObject node = dagPath.node();
            ss << "/" << dagPath.fullPathName().asChar() + 1;
            if (!node.hasFn(MFn::kTransform)) {
                ss << "/Shape";
            }
            auto pathName = ss.str();
            std::replace(pathName.begin(), pathName.end(), '|', '_');
            std::replace(pathName.begin(), pathName.end(), ':', '_');
            path = SdfPath(pathName);
        } else {
            return SdfPath();
        }
    } else {
        path = PxrUsdMayaUtil::MDagPathToUsdPath(dagPath, false);
    }
    return rootOverridePath(mArgs, path);
}

bool usdWriteJobCtx::openFile(const std::string& filename, bool append)
{
    ArResolverContext resolverCtx = ArGetResolver().GetCurrentContext();
    if (append) {
        mStage = UsdStage::Open(SdfLayer::FindOrOpen(filename), resolverCtx);
        if (!mStage) {
            MGlobal::displayError("Failed to open stage file " + MString(filename.c_str()));
            return false;
        }
    } else {
        mStage = UsdStage::CreateNew(filename, resolverCtx);
        if (!mStage) {
            MGlobal::displayError("Failed to create stage file " + MString(filename.c_str()));
            return false;
        }
    }

    if (mArgs.exportInstances) {
        SdfPath instancesPath(instancesScopeName);
        mInstancesPrim = mStage->OverridePrim(rootOverridePath(mArgs, instancesPath));
    }

    return true;
}

void usdWriteJobCtx::processInstances()
{
    if (mArgs.exportInstances) {
        if (mNoInstances) {
            mStage->RemovePrim(mInstancesPrim.GetPrimPath());
        } else {
            mInstancesPrim.SetSpecifier(SdfSpecifierOver);
        }
    }
}

MayaPrimWriterPtr usdWriteJobCtx::createPrimWriter(
    const MDagPath& curDag)
{
    return _createPrimWriter(curDag, false);
}

MayaPrimWriterPtr usdWriteJobCtx::_createPrimWriter(
    const MDagPath& curDag, bool instanceSource)
{
    MObject ob = curDag.node();

    // Check whether a user prim writer exists for the node first, since plugin
    // nodes may provide the same function sets as native Maya nodes. If a
    // writer can't be found, we'll fall back on the standard writers below.
    if (ob.hasFn(MFn::kPluginDependNode) && ob.hasFn(MFn::kDagNode) && ob.hasFn(MFn::kDependencyNode)) {
        MFnDependencyNode depNodeFn(ob);
        MPxNode *pxNode = depNodeFn.userNode();

        std::string mayaTypeName(pxNode->typeName().asChar());

        if (PxrUsdMayaPrimWriterRegistry::WriterFactoryFn primWriterFactory =
                PxrUsdMayaPrimWriterRegistry::Find(mayaTypeName)) {
            MayaPrimWriterPtr primPtr(primWriterFactory(
                curDag, getUsdPathFromDagPath(curDag, instanceSource), instanceSource, *this));
            if (primPtr && primPtr->isValid()) {
                // We found a registered user prim writer that handles this node
                // type, so return now.
                return primPtr;
            }
        }
    }

    if (ob.hasFn(MFn::kTransform) || ob.hasFn(MFn::kLocator) ||
        (mArgs.exportInstances && curDag.isInstanced() && !instanceSource)) {
        MayaTransformWriterPtr primPtr(new MayaTransformWriter(curDag, getUsdPathFromDagPath(curDag, instanceSource), instanceSource, *this));
        if (primPtr->isValid()) {
            return primPtr;
        }
    } else if (ob.hasFn(MFn::kMesh)) {
        MayaMeshWriterPtr primPtr(new MayaMeshWriter(curDag, getUsdPathFromDagPath(curDag, instanceSource), instanceSource, *this));
        if (primPtr->isValid()) {
            return primPtr;
        }
    } else if (ob.hasFn(MFn::kNurbsCurve)) {
        MayaNurbsCurveWriterPtr primPtr(new MayaNurbsCurveWriter(curDag, getUsdPathFromDagPath(curDag, instanceSource), instanceSource, *this));
        if (primPtr->isValid()) {
            return primPtr;
        }
    } else if (ob.hasFn(MFn::kNurbsSurface)) {
        MayaNurbsSurfaceWriterPtr primPtr(new MayaNurbsSurfaceWriter(curDag, getUsdPathFromDagPath(curDag, instanceSource), instanceSource, *this));
        if (primPtr->isValid()) {
            return primPtr;
        }
    } else if (ob.hasFn(MFn::kCamera)) {
        MayaCameraWriterPtr primPtr(new MayaCameraWriter(curDag, getUsdPathFromDagPath(curDag, false), *this));
        if (primPtr->isValid()) {
            return primPtr;
        }
    }

    return nullptr;
}

PXR_NAMESPACE_CLOSE_SCOPE
