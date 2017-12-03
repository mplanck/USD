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
#ifndef PXRUSDMAYA_USD_IMPORT_H
#define PXRUSDMAYA_USD_IMPORT_H

#include "pxr/pxr.h"
#include "usdMaya/api.h"
#include <maya/MArgList.h>
#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>

#include <string>

PXR_NAMESPACE_OPEN_SCOPE


class usdReadJob;

class usdImport : public MPxCommand
{
  public:
    PXRUSDMAYA_API
    usdImport(const std::string& assemblyTypeName,
              const std::string& proxyShapeTypeName);
    PXRUSDMAYA_API
    virtual ~usdImport();
    
    PXRUSDMAYA_API
    virtual MStatus doIt(const MArgList& args);
    PXRUSDMAYA_API
    virtual MStatus redoIt();
    PXRUSDMAYA_API
    virtual MStatus undoIt();
    virtual bool  isUndoable () const { return true; };


    PXRUSDMAYA_API
    static MSyntax  createSyntax();
    PXRUSDMAYA_API
    static void* creator(const std::string& assemblyTypeName,
                         const std::string& proxyShapeTypeName);

  private:
    usdReadJob *mUsdReadJob;

    const std::string _assemblyTypeName;
    const std::string _proxyShapeTypeName;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // PXRUSDMAYA_USD_IMPORT_H
