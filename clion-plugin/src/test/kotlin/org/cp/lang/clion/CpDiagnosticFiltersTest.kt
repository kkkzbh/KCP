package org.cp.lang.clion

import com.intellij.lang.LanguageParserDefinitions
import com.intellij.lang.injection.InjectedLanguageManager
import com.intellij.openapi.fileTypes.FileTypeManager
import com.intellij.psi.PsiDocumentManager
import com.intellij.psi.PsiErrorElement
import com.intellij.testFramework.fixtures.BasePlatformTestCase

class CpDiagnosticFiltersTest : BasePlatformTestCase() {
    private val parserDefinition = CpParserDefinition()

    override fun setUp() {
        super.setUp()
        LanguageParserDefinitions.INSTANCE.addExplicitExtension(CpLanguage, parserDefinition)
    }

    override fun tearDown() {
        try {
            LanguageParserDefinitions.INSTANCE.removeExplicitExtension(CpLanguage, parserDefinition)
        } finally {
            super.tearDown()
        }
    }

    fun testMarkdownFenceIsRecognizedAsNonDiagnosticCpFragment() {
        val markdownType = FileTypeManager.getInstance().getFileTypeByExtension("md")
        assertEquals("Markdown", markdownType.name)

        val host = myFixture.configureByText(
            markdownType,
            """
            ```cp
            let data: array<i32, 4> = [1, 2, 3, 4];
            ```
            """.trimIndent(),
        )
        PsiDocumentManager.getInstance(project).commitAllDocuments()

        val injectedFile = findInjectedCpFile(host.text.indexOf("data"))

        assertTrue(CpInjectedFragmentPolicy.isCpMarkdownCodeFence(injectedFile))
        assertTrue(CpExternalAnnotatorsFilter().isProhibited(CpExternalAnnotator(), injectedFile))
        assertFalse(
            CpPsiErrorFilter().shouldHighlightErrorElement(
                injectedFile.collectPsiErrors().single { it.errorDescription == "expected top-level function" },
            ),
        )
    }

    fun testRegularCpFileSuppressesPsiParserErrorsAndKeepsExternalAnnotator() {
        val file = myFixture.configureByText(CpFileType.INSTANCE, "let data = 1;\n")
        val error = file.collectPsiErrors().single { it.errorDescription == "expected top-level function" }

        assertFalse(CpInjectedFragmentPolicy.isCpMarkdownCodeFence(file))
        assertFalse(CpExternalAnnotatorsFilter().isProhibited(CpExternalAnnotator(), file))
        assertFalse(CpPsiErrorFilter().shouldHighlightErrorElement(error))
    }

    private fun findInjectedCpFile(hostOffset: Int) =
        InjectedLanguageManager.getInstance(project)
            .findInjectedElementAt(myFixture.file, hostOffset)
            ?.containingFile
            ?.takeIf { it.language == CpLanguage }
            ?: error("cp injected file was not created")

    private fun com.intellij.psi.PsiElement.collectPsiErrors(): List<PsiErrorElement> {
        val result = mutableListOf<PsiErrorElement>()
        collectPsiErrors(result)
        return result
    }

    private fun com.intellij.psi.PsiElement.collectPsiErrors(result: MutableList<PsiErrorElement>) {
        if (this is PsiErrorElement) {
            result += this
        }
        for (child in children) {
            child.collectPsiErrors(result)
        }
    }
}
