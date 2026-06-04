package org.cp.lang.clion

import com.intellij.lang.Language
import com.intellij.openapi.fileTypes.LanguageFileType
import javax.swing.Icon

object CpLanguage : Language("cp")

class CpFileType private constructor() : LanguageFileType(CpLanguage) {
    override fun getName(): String = "KCP"

    override fun getDescription(): String = "KCP 源文件"

    override fun getDefaultExtension(): String = "cp"

    override fun getIcon(): Icon? = null

    companion object {
        @JvmField
        val INSTANCE = CpFileType()
    }
}
