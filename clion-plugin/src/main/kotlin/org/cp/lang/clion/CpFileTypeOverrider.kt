package org.cp.lang.clion

import com.intellij.openapi.fileTypes.FileType
import com.intellij.openapi.fileTypes.impl.FileTypeOverrider
import com.intellij.openapi.vfs.VirtualFile

class CpFileTypeOverrider : FileTypeOverrider {
    override fun getOverriddenFileType(file: VirtualFile): FileType? {
        return if(file.extension == CpFileType.INSTANCE.defaultExtension) {
            CpFileType.INSTANCE
        } else {
            null
        }
    }
}
