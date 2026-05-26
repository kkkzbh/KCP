package org.cp.lang.clion

import com.intellij.codeInsight.highlighting.HighlightErrorFilter
import com.intellij.lang.ExternalAnnotatorsFilter
import com.intellij.lang.annotation.ExternalAnnotator
import com.intellij.lang.injection.InjectedLanguageManager
import com.intellij.psi.PsiErrorElement
import com.intellij.psi.PsiFile

object CpInjectedFragmentPolicy {
    fun isCpMarkdownCodeFence(file: PsiFile?): Boolean {
        if (file == null || file.language != CpLanguage) {
            return false
        }

        val injectionManager = InjectedLanguageManager.getInstance(file.project)
        if (!injectionManager.isInjectedFragment(file)) {
            return false
        }

        val hostFile = injectionManager.getTopLevelFile(file)
        if (!hostFile.isMarkdownFile()) {
            return false
        }

        val host = injectionManager.getInjectionHost(file) ?: return false
        return host.javaClass.name.contains("MarkdownCodeFence")
    }

    private fun PsiFile.isMarkdownFile(): Boolean =
        fileType.name == "Markdown" || language.id == "Markdown"
}

class CpPsiErrorFilter : HighlightErrorFilter() {
    override fun shouldHighlightErrorElement(element: PsiErrorElement): Boolean =
        element.containingFile?.language != CpLanguage
}

class CpExternalAnnotatorsFilter : ExternalAnnotatorsFilter {
    override fun isProhibited(annotator: ExternalAnnotator<*, *>, file: PsiFile): Boolean =
        annotator is CpExternalAnnotator && CpInjectedFragmentPolicy.isCpMarkdownCodeFence(file)
}
