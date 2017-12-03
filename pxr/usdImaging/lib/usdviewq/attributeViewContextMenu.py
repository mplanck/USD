#
# Copyright 2016 Pixar
#
# Licensed under the Apache License, Version 2.0 (the "Apache License")
# with the following modification; you may not use this file except in
# compliance with the Apache License and the following modification to it:
# Section 6. Trademarks. is deleted and replaced with:
#
# 6. Trademarks. This License does not grant permission to use the trade
#    names, trademarks, service marks, or product names of the Licensor
#    and its affiliates, except as required to comply with Section 4(c) of
#    the License and to reproduce the content of the NOTICE file.
#
# You may obtain a copy of the Apache License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the Apache License with the above modification is
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the Apache License for the specific
# language governing permissions and limitations under the Apache License.
#
from qt import QtGui, QtWidgets, QtCore
from usdviewContextMenuItem import UsdviewContextMenuItem
from common import (INDEX_PROPNAME, INDEX_PROPTYPE, INDEX_PROPVAL,
                    ATTR_PLAIN_TYPE_ROLE, REL_PLAIN_TYPE_ROLE, ATTR_WITH_CONN_TYPE_ROLE,
                    REL_WITH_TARGET_TYPE_ROLE, CMP_TYPE_ROLE, CONN_TYPE_ROLE, TARGET_TYPE_ROLE)

#
# Specialized context menu for running commands in the attribute viewer.
#
class AttributeViewContextMenu(QtWidgets.QMenu):

    def __init__(self, parent, item):
        QtWidgets.QMenu.__init__(self, parent)
        self._menuItems = _GetContextMenuItems(parent, item)

        for menuItem in self._menuItems:
            # create menu actions
            if menuItem.isValid() and menuItem.ShouldDisplay():
                action = self.addAction(menuItem.GetText(), menuItem.RunCommand)

                # set enabled
                if not menuItem.IsEnabled():
                    action.setEnabled(False)


def _GetContextMenuItems(mainWindow, item):
    return [ # Root selection methods
             CopyAttributeNameMenuItem(mainWindow, item),
             CopyAttributeValueMenuItem(mainWindow, item),
             CopyAllTargetPathsMenuItem(mainWindow, item),
             SelectAllTargetPathsMenuItem(mainWindow, item),

             # Individual/multi target selection menus
             CopyTargetPathMenuItem(mainWindow, item),
             SelectTargetPathMenuItem(mainWindow, item)]

#
# The base class for propertyview context menu items.
#
class AttributeViewContextMenuItem(UsdviewContextMenuItem):
    def __init__(self, mainWindow, item):
        self._mainWindow = mainWindow
        self._item = item
        self._role = self._item.data(INDEX_PROPTYPE, QtCore.Qt.ItemDataRole.WhatsThisRole)
        self._name = self._item.text(INDEX_PROPNAME) if self._item else ""
        self._value = self._item.text(INDEX_PROPVAL) if self._item else ""

    def IsEnabled(self):
        return True

    def ShouldDisplay(self):
        return True

    def GetText(self):
        return ""

    def RunCommand(self):
        pass

#
# Copy the attribute's name to clipboard.
#
class CopyAttributeNameMenuItem(AttributeViewContextMenuItem):
    def ShouldDisplay(self):
        return self._role != TARGET_TYPE_ROLE and self._role != CONN_TYPE_ROLE

    def GetText(self):
        return "Copy Property Name"

    def RunCommand(self):
        if self._name == "":
            return 

        cb = QtWidgets.QApplication.clipboard()
        cb.setText(self._name, QtGui.QClipboard.Selection)
        cb.setText(self._name, QtGui.QClipboard.Clipboard)

#
# Copy the attribute's value to clipboard.
#
class CopyAttributeValueMenuItem(AttributeViewContextMenuItem):
    def ShouldDisplay(self):
        return self._role != TARGET_TYPE_ROLE and self._role != CONN_TYPE_ROLE

    def GetText(self):
        return "Copy Property Value"

    def RunCommand(self):
        if self._value == "":
            return

        cb = QtWidgets.QApplication.clipboard()
        cb.setText(self._value, QtGui.QClipboard.Selection)
        cb.setText(self._value, QtGui.QClipboard.Clipboard)

# --------------------------------------------------------------------
# Individual target selection menus
# --------------------------------------------------------------------

#
# Copy the target path to clipboard.
#
class CopyTargetPathMenuItem(AttributeViewContextMenuItem):
    def ShouldDisplay(self):
        return self._role == TARGET_TYPE_ROLE or self._role == CONN_TYPE_ROLE

    def GetText(self):
        return "Copy Target Path As Text"

    def GetSelectedOfType(self):
        getRole = lambda s: s.data(INDEX_PROPTYPE, QtCore.Qt.ItemDataRole.WhatsThisRole)
        return [s for s in self._item.treeWidget().selectedItems() \
                    if getRole(s) == TARGET_TYPE_ROLE or getRole(s) == CONN_TYPE_ROLE]

    def RunCommand(self):
        if not self._item:
            return

        value = ", ".join([s.text(INDEX_PROPNAME) for s in self.GetSelectedOfType()]) 
        cb = QtWidgets.QApplication.clipboard()
        cb.setText(value, QtGui.QClipboard.Selection)
        cb.setText(value, QtGui.QClipboard.Clipboard)

#
# Jump to the target path in the prim browser
# This will include all other highlighted paths of this type, if any.
#
class SelectTargetPathMenuItem(CopyTargetPathMenuItem):
    def GetText(self):
        return "Select Target Path"

    def RunCommand(self):
        self._mainWindow.jumpToTargetPaths([s.text(INDEX_PROPNAME) \
                                            for s in self.GetSelectedOfType()])

# --------------------------------------------------------------------
# Target owning property selection menus
# --------------------------------------------------------------------

# 
# Jump to all target paths under the selected attribute
#
class SelectAllTargetPathsMenuItem(AttributeViewContextMenuItem):
    def ShouldDisplay(self):
        return self._role == REL_WITH_TARGET_TYPE_ROLE or self._role == ATTR_WITH_CONN_TYPE_ROLE

    def IsEnabled(self):
        if not self._item:
            return False 

        # Disable the menu if there are no targets
        # for this rel/attribute connection
        return self._item.childCount() != 0

    def GetText(self):
        return "Select Target Path(s)"

    def RunCommand(self):
        if not self._item:
            return

        # Deselect the parent and jump to all of its children
        self._item.setSelected(False)
        self._mainWindow.jumpToTargetPaths([self._item.child(i).text(INDEX_PROPNAME) \
                                             for i in range(0, self._item.childCount())])

# 
# Copy all target paths under the currently selected relationship to the clipboard
#
class CopyAllTargetPathsMenuItem(SelectAllTargetPathsMenuItem):
    def GetText(self):
        return "Copy Target Path(s) As Text"

    def RunCommand(self):
        if not self._item:
            return

        value = ", ".join([self._item.child(i).text(INDEX_PROPNAME) \
                            for i in range(0, self._item.childCount())]) 
        cb = QtWidgets.QApplication.clipboard()
        cb.setText(value, QtGui.QClipboard.Selection)
        cb.setText(value, QtGui.QClipboard.Clipboard)
