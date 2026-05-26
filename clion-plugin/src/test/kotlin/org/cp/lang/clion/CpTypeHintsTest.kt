package org.cp.lang.clion

import com.intellij.lang.LanguageParserDefinitions
import com.intellij.openapi.application.ApplicationManager
import com.intellij.openapi.fileTypes.FileTypeManager
import com.intellij.psi.PsiFile
import com.intellij.testFramework.fixtures.BasePlatformTestCase
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import java.nio.file.Files
import java.nio.file.Path

class CpTypeHintsTest : BasePlatformTestCase() {
    private val parserDefinition = CpParserDefinition()

    override fun setUp() {
        super.setUp()
        LanguageParserDefinitions.INSTANCE.addExplicitExtension(CpLanguage, parserDefinition)
        ApplicationManager.getApplication().runWriteAction {
            FileTypeManager.getInstance().associateExtension(CpFileType.INSTANCE, "cp")
        }
    }

    override fun tearDown() {
        try {
            ApplicationManager.getApplication().runWriteAction {
                FileTypeManager.getInstance().removeAssociatedExtension(CpFileType.INSTANCE, "cp")
            }
            LanguageParserDefinitions.INSTANCE.removeExplicitExtension(CpLanguage, parserDefinition)
        } finally {
            super.tearDown()
        }
    }

    fun testInfersLocalLiteralStructAndCallTypes() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            struct box {
                value: i32;
            }

            answer() -> i32
            {
                return 42;
            }

            main() -> i32
            {
                let count = 1;
                let ok = true;
                let item = box{ .value = count };
                let result = answer();
                return result;
            }
            """.trimIndent(),
        )

        val hints = CpTypeHintEngine.items(myFixture.file).associate { it.name to it.type }

        assertEquals("i32", hints["count"])
        assertEquals("bool", hints["ok"])
        assertEquals("box", hints["item"])
        assertEquals("i32", hints["result"])
    }

    fun testDoesNotShowHintForExplicitLocalType() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            main()
            {
                let explicit: i32 = 1;
                let inferred = explicit;
            }
            """.trimIndent(),
        )

        val hints = CpTypeHintEngine.items(myFixture.file).map { it.name }

        assertFalse("explicit" in hints)
        assertTrue("inferred" in hints)
    }

    fun testInfersActualLexerLabLocalState() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            Files.readString(repoRoot().resolve("lab/lexer/lexer.cp")),
        )

        val hints = CpTypeHintEngine.items(myFixture.file).associate { it.name to it.type }

        assertEquals("lexer", hints["state"])
    }

    fun testInfersImportedImplMemberAndFieldTypes() {
        myFixture.addFileToProject(
            "source/source.cp",
            """
            export module source;

            export struct source_span {
                start: usize;
                end: usize;
            }
            """.trimIndent(),
        )
        myFixture.addFileToProject(
            "lexer/token.cp",
            """
            export module lexer.token;

            import source;

            export enum token_kind {
                l_paren;
                r_paren;
            }

            export struct token {
                kind: token_kind;
                span: source_span;
            }
            """.trimIndent(),
        )
        myFixture.addFileToProject(
            "parser/ast.cp",
            """
            export module parser.ast;

            export struct function_id {
                value: usize;
            }

            export struct stmt_id {
                value: usize;
            }
            """.trimIndent(),
        )
        myFixture.addFileToProject(
            "parser/state.cp",
            """
            export module parser.state;

            import lexer.token;
            import parser.ast;

            export struct parser {
                index: usize;
            }

            impl parser {
                peek(self const&) -> token
                {
                    return token{};
                }

                expect(self&, kind: token_kind) -> optional<token>
                {
                    return optional<token>::none;
                }

                expect_identifier(self&) -> optional<token>
                {
                    return optional<token>::none;
                }
            }
            """.trimIndent(),
        )
        myFixture.addFileToProject(
            "parser/statement.cp",
            """
            export module parser.statement;

            import parser.ast;
            import parser.state;

            impl parser {
                parse_block(self&) -> optional<stmt_id>
                {
                    return optional<stmt_id>::none;
                }
            }
            """.trimIndent(),
        )
        val program = myFixture.addFileToProject(
            "parser/program.cp",
            """
            export module parser.program;

            import lexer.token;
            import parser.ast;
            import parser.state;
            import parser.statement;

            impl parser {
                parse_function(self&) -> optional<function_id>
                {
                    let first = peek();
                    let start = first.span;
                    let name = expect_identifier();
                    let open = expect(token_kind::l_paren);
                    let body = parse_block();
                    return optional<function_id>::none;
                }
            }
            """.trimIndent(),
        )
        myFixture.configureFromExistingVirtualFile(program.virtualFile)
        seedSemanticCache(myFixture.file)

        val hints = CpTypeHintEngine.items(myFixture.file).associate { it.name to it.type }

        assertEquals("token", hints["first"])
        assertEquals("source_span", hints["start"])
        assertEquals("optional<token>", hints["name"])
        assertEquals("optional<token>", hints["open"])
        assertEquals("optional<stmt_id>", hints["body"])
    }

    fun testTypeHintCacheInvalidatesWhenImportedSnapshotChanges() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            import math;

            main()
            {
                let result = answer();
            }
            """.trimIndent(),
        )
        val activePath = myFixture.file.virtualFile.path
        val importedPath = "$activePath.math.cp"
        storeImportClosure(
            activePath,
            importedPath,
            """
            export module math;

            answer() -> i32
            {
                return 1;
            }
            """.trimIndent(),
        )
        val first = CpTypeHintEngine.items(myFixture.file).associate { it.name to it.type }

        storeImportClosure(
            activePath,
            importedPath,
            """
            export module math;

            answer() -> f64
            {
                return 2.0;
            }
            """.trimIndent(),
        )
        val second = CpTypeHintEngine.items(myFixture.file).associate { it.name to it.type }

        assertEquals("i32", first["result"])
        assertEquals("f64", second["result"])
    }

    fun testInfersBuiltinEscapeBuiltinImplAndUfcsTypes() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            raw(value: i32) -> i32
            {
                return value;
            }

            impl i32 {
                bump(self const&) -> i32
                {
                    return self;
                }
            }

            main() -> i32
            {
                let value = 2;
                let escaped = builtin(value + 3);
                let bumped = value.bump();
                let via_ufcs = value.raw();
                return escaped + bumped + via_ufcs;
            }
            """.trimIndent(),
        )

        val hints = CpTypeHintEngine.items(myFixture.file).associate { it.name to it.type }

        assertEquals("i32", hints["escaped"])
        assertEquals("i32", hints["bumped"])
        assertEquals("i32", hints["via_ufcs"])
    }

    fun testDeclarativeProviderCreatesCollectorForCpFiles() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            main()
            {
                let value = 1;
            }
            """.trimIndent(),
        )

        assertNotNull(CpTypeHintsProvider().createCollector(myFixture.file, myFixture.editor))
    }

    private fun repoRoot(): Path =
        Path.of(System.getProperty("cp.repo.root") ?: "..").normalize().toAbsolutePath()

    private fun seedSemanticCache(file: PsiFile) {
        val request = CpProjectSnapshotCollector.collect(file, file.text)
            ?: error("cp project snapshot was not created")
        CpSemanticCache.get(project).store(request, CpExternalAnnotator.emptyInspectionResult(), cpModificationCount(project))
    }

    private fun storeImportClosure(activePath: String, importedPath: String, importedText: String) {
        CpSemanticCache.get(project).store(
            CpInspectionRequest(
                activeFile = activePath,
                files = listOf(
                    CpInspectionFile(activePath, myFixture.file.text),
                    CpInspectionFile(importedPath, importedText),
                ),
            ),
            CpExternalAnnotator.emptyInspectionResult(),
            cpModificationCount(project),
        )
    }
}
