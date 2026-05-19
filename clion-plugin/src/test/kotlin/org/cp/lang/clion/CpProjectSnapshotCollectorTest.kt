package org.cp.lang.clion

import com.intellij.lang.LanguageParserDefinitions
import com.intellij.openapi.command.WriteCommandAction
import com.intellij.openapi.fileEditor.FileDocumentManager
import com.intellij.testFramework.fixtures.BasePlatformTestCase

class CpProjectSnapshotCollectorTest : BasePlatformTestCase() {
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

    fun testProjectSnapshotUsesUnsavedDocumentTextForDependencies() {
        val dependency = myFixture.addFileToProject(
            "lib.cp",
            """
            export module lib;
            value() -> i32 { return 1; }
            """.trimIndent(),
        )
        val active = myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            import lib;
            main() -> i32 { return value(); }
            """.trimIndent(),
        )
        val document = FileDocumentManager.getInstance().getDocument(dependency.virtualFile)
            ?: error("dependency document was not created")

        WriteCommandAction.runWriteCommandAction(project) {
            document.setText(
                """
                export module lib;
                value() -> i32 { return 2; }
                """.trimIndent(),
            )
        }

        val request = CpProjectSnapshotCollector.collect(active, active.text)
            ?: error("cp project snapshot was not created")

        assertEquals(
            "export module lib;\nvalue() -> i32 { return 2; }",
            request.files.single { it.path.endsWith("/lib.cp") }.text,
        )
    }
}
