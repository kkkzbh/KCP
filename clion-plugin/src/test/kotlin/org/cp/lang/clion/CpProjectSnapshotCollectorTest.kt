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

    fun testCachedProjectSnapshotResolveStillUpdatesKnownDependencyText() {
        val mainPath = "/project/main.cp"
        val libPath = "/project/lib.cp"
        val main = """
            import lib;
            main() -> i32 { return value(); }
        """.trimIndent()
        val first = CpProjectSnapshotCollector.buildRequest(
            activePath = mainPath,
            activeText = main,
            projectFiles = listOf(
                CpSnapshotSource(mainPath, main),
                CpSnapshotSource(
                    libPath,
                    """
                    export module lib;
                    value() -> i32 { return 1; }
                    """.trimIndent(),
                ),
            ),
        )
        val second = CpProjectSnapshotCollector.buildRequest(
            activePath = mainPath,
            activeText = main,
            projectFiles = listOf(
                CpSnapshotSource(mainPath, main),
                CpSnapshotSource(
                    libPath,
                    """
                    export module lib;
                    value() -> i32 { return 2; }
                    """.trimIndent(),
                ),
            ),
        )

        assertEquals(
            "export module lib;\nvalue() -> i32 { return 1; }",
            first.files.single { it.path == libPath }.text,
        )
        assertEquals(
            "export module lib;\nvalue() -> i32 { return 2; }",
            second.files.single { it.path == libPath }.text,
        )
    }

    fun testSemanticCacheCurrentSurvivesUnrelatedPsiModification() {
        val active = myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            main() -> i32
            {
                return 1;
            }
            """.trimIndent(),
        )
        val activePath = active.virtualFile.path
        val cache = CpSemanticCache.get(project)
        cache.store(
            CpInspectionRequest(
                activeFile = activePath,
                files = listOf(CpInspectionFile(activePath, active.text)),
            ),
            CpExternalAnnotator.emptyInspectionResult(),
            cpModificationCount(project),
        )

        myFixture.addFileToProject(
            "unrelated.cp",
            """
            main() -> i32
            {
                return 2;
            }
            """.trimIndent(),
        )

        assertNotNull(cache.current(active))
    }

    fun testSemanticCacheCurrentInvalidatesWhenDependencyDocumentChanges() {
        val dependencyText = """
            export module dep;
            value() -> i32 { return 1; }
        """.trimIndent()
        val dependency = myFixture.addFileToProject(
            "dep.cp",
            dependencyText,
        )
        val active = myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            import dep;
            main() -> i32 { return value(); }
            """.trimIndent(),
        )
        val activePath = active.virtualFile.path
        val dependencyPath = dependency.virtualFile.path
        val cache = CpSemanticCache.get(project)
        cache.store(
            CpInspectionRequest(
                activeFile = activePath,
                files = listOf(
                    CpInspectionFile(activePath, active.text),
                    CpInspectionFile(dependencyPath, dependencyText),
                ),
            ),
            CpExternalAnnotator.emptyInspectionResult(),
            cpModificationCount(project),
        )

        val document = FileDocumentManager.getInstance().getDocument(dependency.virtualFile)
            ?: error("dependency document was not created")
        WriteCommandAction.runWriteCommandAction(project) {
            document.setText(
                """
                export module dep;
                value() -> i32 { return 2; }
                """.trimIndent(),
            )
        }

        assertNull(cache.current(active))
    }
}
