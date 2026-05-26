package org.cp.lang.clion

import com.intellij.lang.LanguageParserDefinitions
import com.intellij.codeInsight.navigation.actions.GotoDeclarationAction
import com.intellij.codeInsight.navigation.actions.GotoDeclarationHandler
import com.intellij.codeInsight.navigation.actions.GotoDeclarationOrUsageHandler2
import com.intellij.openapi.application.ApplicationManager
import com.intellij.openapi.fileTypes.FileTypeManager
import com.intellij.navigation.DirectNavigationProvider
import com.intellij.psi.PsiElement
import com.intellij.psi.search.searches.ReferencesSearch
import com.intellij.testFramework.ExtensionTestUtil
import com.intellij.testFramework.fixtures.BasePlatformTestCase
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue

class CpNavigationTest : BasePlatformTestCase() {
    private val parserDefinition = CpParserDefinition()
    private val handler = CpGotoDeclarationHandler()
    private val directNavigationProvider = CpDirectNavigationProvider()

    override fun setUp() {
        super.setUp()
        LanguageParserDefinitions.INSTANCE.addExplicitExtension(CpLanguage, parserDefinition)
        ExtensionTestUtil.addExtensions(GotoDeclarationHandler.EP_NAME, listOf(handler), testRootDisposable)
        ExtensionTestUtil.addExtensions(DirectNavigationProvider.EP_NAME, listOf(directNavigationProvider), testRootDisposable)
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

    fun testGotoLocalDeclaration() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            main() -> i32
            {
                let sum: i32 = 1;
                return <caret>sum;
            }
            """.trimIndent(),
        )

        val target = singleTarget()

        assertEquals(CpElements.LOCAL_DECLARATION, target.cpElementType())
        assertEquals("sum", target.text)
    }

    fun testGotoParameterDeclaration() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            add(value: i32) -> i32
            {
                return <caret>value;
            }
            """.trimIndent(),
        )

        val target = singleTarget()

        assertEquals(CpElements.PARAMETER_NAME, target.cpElementType())
        assertEquals("value", target.text)
    }

    fun testGotoImportedFunctionDeclaration() {
        myFixture.addFileToProject(
            "math.cp",
            """
            export module math;

            export add(left: i32, right: i32) -> i32
            {
                return left + right;
            }
            """.trimIndent(),
        )
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            import math;

            main() -> i32
            {
                return <caret>add(1, 2);
            }
            """.trimIndent(),
        )

        val target = singleTarget()

        assertEquals(CpElements.FUNCTION_NAME, target.cpElementType())
        assertEquals("add", target.text)
        assertEquals("math.cp", target.containingFile.name)
    }

    fun testGotoImportedFunctionDeclarationWhenCaretIsAtIdentifierEnd() {
        myFixture.addFileToProject(
            "math.cp",
            """
            export module math;

            export add(left: i32, right: i32) -> i32
            {
                return left + right;
            }
            """.trimIndent(),
        )
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            import math;

            main() -> i32
            {
                return add<caret>(1, 2);
            }
            """.trimIndent(),
        )

        val target = singleTarget()

        assertEquals(CpElements.FUNCTION_NAME, target.cpElementType())
        assertEquals("add", target.text)
        assertEquals("math.cp", target.containingFile.name)
    }

    fun testGotoUnexportedImportedFunctionDeclaration() {
        myFixture.addFileToProject(
            "math.cp",
            """
            export module math.x;

            add(x: i32, y: i32) -> i32
            {
                return x + y;
            }
            """.trimIndent(),
        )
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            import math.x;

            main() -> i32
            {
                return <caret>add(1, 2);
            }
            """.trimIndent(),
        )

        val target = singleTarget()

        assertEquals(CpElements.FUNCTION_NAME, target.cpElementType())
        assertEquals("add", target.text)
        assertEquals("math.cp", target.containingFile.name)
    }

    fun testGotoImportedMemberCallsInsideStructInitializer() {
        val state = myFixture.addFileToProject(
            "lexer/state.cp",
            """
            export module lexer.state;

            export struct diagnostic {}
            export struct token {}
            export struct lexer {
                diagnostics: vector<diagnostic>;
            }

            impl lexer {
                take_diagnostics(self&) -> vector<diagnostic>
                {
                    return diagnostics;
                }

                tokenize_all(self&) -> vector<token>
                {
                    return vector<token>{};
                }
            }
            """.trimIndent(),
        )
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            export module lexer;

            import lexer.state;

            export struct lex_result {
                tokens: vector<token>;
                diagnostics: vector<diagnostic>;
            }

            export lex() -> lex_result
            {
                let state = lexer{};
                return lex_result{
                    .tokens = state.tokenize_all(),
                    .diagnostics = state.take_diagnostics()
                };
            }
            """.trimIndent(),
        )

        assertMemberCallTarget("tokenize_all", state)
        assertMemberCallTarget("take_diagnostics", state)
    }

    fun testReferenceResolveFindsImportedFunctionInNestedModuleLayout() {
        myFixture.addFileToProject(
            "parser/lr/table.cp",
            """
            export module parser.lr.table;

            export struct parser_tables {}

            export build_parser_tables() -> parser_tables
            {
                return parser_tables{};
            }
            """.trimIndent(),
        )
        myFixture.addFileToProject(
            "parser/lr/state.cp",
            """
            export module parser.lr.state;

            import parser.lr.table;

            export struct parser_trace {}

            export parse_with_trace(tables: parser_tables const&) -> parser_trace
            {
                return parser_trace{};
            }
            """.trimIndent(),
        )
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            export module parser.lr.parser;

            import parser.lr.table;
            import parser.lr.state;

            export parse() -> parser_trace
            {
                let tables = <caret>build_parser_tables();
                return parse_with_trace(tables);
            }
            """.trimIndent(),
        )

        warmSemanticCache()
        val reference = myFixture.file.findReferenceAt(myFixture.caretOffset)
        assertNotNull(reference)
        val resolved = reference!!.resolve()

        assertNotNull(resolved)
        assertEquals(CpElements.FUNCTION_NAME, resolved!!.cpElementType())
        assertEquals("build_parser_tables", resolved.text)
        assertEquals("table.cp", resolved.containingFile.name)

        val target = singleTarget()
        assertEquals(resolved, target)
    }

    fun testColdReferenceResolveDoesNotComputeSemanticTarget() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            build_parser_tables() -> parser_tables
            {
                return parser_tables{};
            }

            parse() -> parser_tables
            {
                let tables = <caret>build_parser_tables();
                return tables;
            }
            """.trimIndent(),
        )

        val reference = myFixture.file.findReferenceAt(myFixture.caretOffset)
        assertNotNull(reference)
        val target = reference!!.resolve()

        assertNull(target)
        assertNull(CpSemanticCache.get(project).current(myFixture.file))
    }

    fun testExplicitGotoComputesColdSemanticTarget() {
        myFixture.addFileToProject(
            "lexer/state.cp",
            """
            export module lexer.state;

            export struct diagnostic {}
            export struct lexer {
                diagnostics: vector<diagnostic>;
            }

            impl lexer {
                take_diagnostics(self&) -> vector<diagnostic>
                {
                    return diagnostics;
                }
            }
            """.trimIndent(),
        )
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            export module lexer;

            import lexer.state;

            export lex()
            {
                let state = lexer{};
                return state.<caret>take_diagnostics();
            }
            """.trimIndent(),
        )

        val target = handler.getGotoDeclarationTargets(caretElement(), myFixture.caretOffset, myFixture.editor)
            ?.single()

        assertNotNull(target)
        assertEquals(CpElements.FUNCTION_NAME, target!!.cpElementType())
        assertEquals("take_diagnostics", target.text)
        assertEquals("state.cp", target.containingFile.name)
    }

    fun testJetBrainsGotoActionComputesColdSemanticTarget() {
        configureLexerMemberCall()

        assertTrue(GotoDeclarationHandler.EP_NAME.extensionList.any { it is CpGotoDeclarationHandler })
        assertEquals(
            GotoDeclarationOrUsageHandler2.GTDUOutcome.GTD,
            GotoDeclarationOrUsageHandler2.testGTDUOutcomeInNonBlockingReadAction(
                myFixture.editor,
                myFixture.file,
                myFixture.caretOffset,
            ),
        )

        val targets = GotoDeclarationAction.findAllTargetElements(project, myFixture.editor, myFixture.caretOffset)
        assertEquals(1, targets.size)
        val target = targets.single()

        assertEquals(CpElements.FUNCTION_NAME, target.cpElementType())
        assertEquals("take_diagnostics", target.text)
        assertEquals("state.cp", target.containingFile.name)
    }

    fun testExplicitGotoRecomputesWhenCachedResultHasNoMatchingNavigation() {
        configureLexerMemberCall()
        val activePath = myFixture.file.virtualFile.path
        CpSemanticCache.get(project).store(
            CpInspectionRequest(
                activeFile = activePath,
                files = listOf(CpInspectionFile(activePath, myFixture.editor.document.text)),
            ),
            CpInspectionResult(
                accepted = true,
                diagnostics = emptyList(),
                highlights = emptyList(),
                navigation = emptyList(),
            ),
            cpModificationCount(project),
        )

        val target = handler.getGotoDeclarationTargets(caretElement(), myFixture.caretOffset, myFixture.editor)
            ?.single()

        assertNotNull(target)
        assertEquals(CpElements.FUNCTION_NAME, target!!.cpElementType())
        assertEquals("take_diagnostics", target.text)
        assertEquals("state.cp", target.containingFile.name)
    }

    fun testDirectNavigationProviderComputesColdSemanticTarget() {
        configureLexerMemberCall()

        val target = directNavigationProvider.getNavigationElement(caretElement())

        assertNotNull(target)
        assertEquals(CpElements.FUNCTION_NAME, target!!.cpElementType())
        assertEquals("take_diagnostics", target.text)
        assertEquals("state.cp", target.containingFile.name)
    }

    fun testDirectNavigationProviderUsesSemanticResolution() {
        myFixture.addFileToProject(
            "table.cp",
            """
            export module table;

            export build_parser_tables() -> parser_tables
            {
                return parser_tables{};
            }
            """.trimIndent(),
        )
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            import table;

            parse() -> parser_tables
            {
                let tables = <caret>build_parser_tables();
                return tables;
            }
            """.trimIndent(),
        )

        warmSemanticCache()
        val target = CpDirectNavigationProvider().getNavigationElement(caretElement())

        assertNotNull(target)
        assertEquals(CpElements.FUNCTION_NAME, target!!.cpElementType())
        assertEquals("build_parser_tables", target.text)
        assertEquals("table.cp", target.containingFile.name)
    }

    fun testGotoImportModuleDeclaration() {
        myFixture.addFileToProject(
            "math.cp",
            """
            export module math;

            export add(left: i32, right: i32) -> i32
            {
                return left + right;
            }
            """.trimIndent(),
        )
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            import <caret>math;
            """.trimIndent(),
        )

        val target = singleTarget()

        assertEquals(CpElements.MODULE_NAME, target.cpElementType())
        assertEquals("math", target.text)
        assertEquals("math.cp", target.containingFile.name)
    }

    fun testGotoImportModuleDeclarationInAggregateSiblingDirectory() {
        val diagnostic = myFixture.addFileToProject(
            "diagnostic/diagnostic.cp",
            """
            export module diagnostic;

            export struct diagnostic {
                code: str;
            }
            """.trimIndent(),
        )
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            export module preprocessor;

            import source;
            import <caret>diagnostic;
            """.trimIndent(),
        )

        val target = singleTarget()

        assertEquals(CpElements.MODULE_NAME, target.cpElementType())
        assertEquals("diagnostic", target.text)
        assertEquals(diagnostic.virtualFile.path, target.containingFile.virtualFile.path)
    }

    fun testFindUsagesUsesNativeNavigationForImportedFunction() {
        val state = myFixture.addFileToProject(
            "parser/state.cp",
            """
            export module parser.state;

            export struct token {}

            export struct parser {}

            impl parser {
                expect_identifier(self&) -> token
                {
                    return token{};
                }
            }
            """.trimIndent(),
        )
        val statement = myFixture.addFileToProject(
            "parser/statement.cp",
            """
            export module parser.statement;

            import parser.state;

            impl parser {
                parse_var_decl(self&) -> token
                {
                    return expect_identifier();
                }

                parse_block(self&) -> token
                {
                    return expect_identifier();
                }
            }
            """.trimIndent(),
        )
        myFixture.configureFromExistingVirtualFile(statement.virtualFile)
        val target = state.descendants(CpElements.FUNCTION_NAME)
            .single { it.text == "expect_identifier" }

        val references = ReferencesSearch.search(target).findAll()
            .distinctBy { it.element.containingFile.virtualFile.path to it.element.textRange }

        assertEquals(2, references.size)
        assertTrue(references.all { it.element.containingFile.virtualFile.path == statement.virtualFile.path })
    }

    fun testFindUsagesAcceptsDeclarationIdentifierElement() {
        val file = myFixture.addFileToProject(
            "parser/parser.cp",
            """
            build_parser_tables() -> parser_tables
            {
                return parser_tables{};
            }

            parse() -> parser_tables
            {
                let first = build_parser_tables();
                let second = build_parser_tables();
                return second;
            }
            """.trimIndent(),
        )
        myFixture.configureFromExistingVirtualFile(file.virtualFile)
        val target = myFixture.file.descendants(CpElements.FUNCTION_NAME)
            .single { it.text == "build_parser_tables" }
        val targetIdentifier = target.firstChild ?: target

        assertTrue(CpFindUsagesProvider().canFindUsagesFor(targetIdentifier))
        val handler = CpFindUsagesHandlerFactory().createFindUsagesHandler(targetIdentifier, false)
        assertNotNull(handler)
        assertEquals(target, handler!!.psiElement)

        val references = ReferencesSearch.search(handler.psiElement).findAll()
            .distinctBy { it.element.textRange }

        assertEquals(2, references.size)
    }

    fun testCaretReferenceResolvesFunctionCallSemantically() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            build_parser_tables() -> parser_tables
            {
                return parser_tables{};
            }

            parse() -> parser_tables
            {
                let tables = <caret>build_parser_tables();
                return tables;
            }
            """.trimIndent(),
        )

        warmSemanticCache()
        val reference = myFixture.file.findReferenceAt(myFixture.caretOffset)
        assertNotNull(reference)
        val target = reference!!.resolve()

        assertNotNull(target)
        assertEquals(CpElements.FUNCTION_NAME, target!!.cpElementType())
        assertEquals("build_parser_tables", target.text)
    }

    fun testGotoImportedStructDeclarationFromInitializer() {
        val state = myFixture.addFileToProject(
            "parser/state.cp",
            """
            export module parser.state;

            export struct parser {
                index: usize;
            }
            """.trimIndent(),
        )
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            export module parser;

            import parser.state;

            parse()
            {
                let state = <caret>parser{};
                return state;
            }
            """.trimIndent(),
        )

        val target = singleTarget()

        assertEquals(CpElements.TYPE_NAME, target.cpElementType())
        assertEquals("parser", target.text)
        assertEquals(state.virtualFile.path, target.containingFile.virtualFile.path)
    }

    fun testGotoUnqualifiedMemberCallUsesEnclosingImplType() {
        myFixture.addFileToProject(
            "preprocessor/preprocessor.cp",
            """
            export module preprocessor;

            export struct preprocessor {}

            impl preprocessor {
                peek(self const&) -> char
                {
                    return 'x';
                }
            }
            """.trimIndent(),
        )
        val state = myFixture.addFileToProject(
            "parser/state.cp",
            """
            export module parser.state;

            export struct parser {}

            impl parser {
                peek(self const&) -> token
                {
                    return token{};
                }
            }
            """.trimIndent(),
        )
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            export module parser.program;

            import preprocessor;
            import parser.state;

            impl parser {
                parse_program(self&) -> program_id
                {
                    return <caret>peek();
                }
            }
            """.trimIndent(),
        )

        val target = singleTarget()

        assertEquals(CpElements.FUNCTION_NAME, target.cpElementType())
        assertEquals("peek", target.text)
        assertEquals(state.virtualFile.path, target.containingFile.virtualFile.path)
    }

    fun testGotoMemberCallUsesInferredReceiverType() {
        myFixture.addFileToProject(
            "preprocessor/preprocessor.cp",
            """
            export module preprocessor;

            export struct preprocessor {}

            impl preprocessor {
                peek(self const&) -> char
                {
                    return 'x';
                }
            }
            """.trimIndent(),
        )
        val state = myFixture.addFileToProject(
            "parser/state.cp",
            """
            export module parser.state;

            export struct parser {}

            impl parser {
                peek(self const&) -> token
                {
                    return token{};
                }
            }
            """.trimIndent(),
        )
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            export module parser;

            import preprocessor;
            import parser.state;

            parse()
            {
                let state = parser{};
                return state.<caret>peek();
            }
            """.trimIndent(),
        )

        val target = singleTarget()

        assertEquals(CpElements.FUNCTION_NAME, target.cpElementType())
        assertEquals("peek", target.text)
        assertEquals(state.virtualFile.path, target.containingFile.virtualFile.path)
    }

    fun testGotoMemberCallUsesImplicitFieldReceiverType() {
        val arena = myFixture.addFileToProject(
            "parser/ast/arena.cp",
            """
            export module parser.ast.arena;

            export struct expr_id {
                value: usize;
            }

            export struct source_span {
                start: usize;
                end: usize;
            }

            export struct ast_arena {
                expressions: usize;
            }

            impl ast_arena {
                expr_span(self const&, id: expr_id) -> source_span
                {
                    return source_span{};
                }
            }
            """.trimIndent(),
        )
        myFixture.addFileToProject(
            "parser/ast/ast.cp",
            """
            export module parser.ast;

            export import parser.ast.arena;
            """.trimIndent(),
        )
        myFixture.addFileToProject(
            "parser/state.cp",
            """
            export module parser.state;

            import parser.ast;

            export struct parser {
                arena: ast_arena;
            }
            """.trimIndent(),
        )
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            export module parser.expression;

            import parser.state;

            impl parser {
                parse_unary(self&) -> source_span
                {
                    return arena.<caret>expr_span(expr_id{ .value = 0 });
                }
            }
            """.trimIndent(),
        )

        val target = singleTarget()

        assertEquals(CpElements.FUNCTION_NAME, target.cpElementType())
        assertEquals("expr_span", target.text)
        assertEquals(arena.virtualFile.path, target.containingFile.virtualFile.path)
    }

    fun testGotoTypeAndFieldDeclarations() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            struct item {
                value: i32;
            }

            main() -> i32
            {
                let current: <caret>item = item{ .value = 1 };
                return current.value;
            }
            """.trimIndent(),
        )

        val typeTarget = singleTarget()

        assertEquals(CpElements.TYPE_NAME, typeTarget.cpElementType())
        assertEquals("item", typeTarget.text)

        val member = myFixture.file.descendants(CpElements.MEMBER_NAME).single { it.text == "value" }
        val fieldTarget = singleTarget(member.firstChild ?: member)

        assertEquals(CpElements.FIELD_DECLARATION, fieldTarget.cpElementType())
        assertEquals("value", fieldTarget.text)
    }

    fun testGotoMemberFunctionDeclarationUsesSemanticTarget() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            struct box {
                value: i32;
            }

            impl box {
                get(self const&) -> i32
                {
                    return self.value;
                }
            }

            main() -> i32
            {
                let item = box{ .value = 1 };
                return item.<caret>get();
            }
            """.trimIndent(),
        )

        val target = singleTarget()

        assertEquals(CpElements.FUNCTION_NAME, target.cpElementType())
        assertEquals("get", target.text)
    }

    private fun singleTarget(element: PsiElement = caretElement()): PsiElement {
        warmSemanticCache()
        val targets = handler.getGotoDeclarationTargets(element, myFixture.caretOffset, myFixture.editor)
        assertNotNull("source=${element.text}:${element.cpElementType()} parent=${element.parent?.text}:${element.parent?.cpElementType()}", targets)
        assertEquals(1, targets!!.size)
        return targets.single()
    }

    private fun warmSemanticCache() {
        CpSemanticCache.get(project).computeNow(myFixture.file, myFixture.editor.document.text)
    }

    private fun caretElement(): PsiElement =
        myFixture.file.findElementAt(myFixture.caretOffset) ?: error("caret element is missing")

    private fun assertMemberCallTarget(name: String, targetFile: PsiElement) {
        val element = myFixture.file.descendants(CpElements.MEMBER_NAME)
            .single { it.text == name }
        myFixture.editor.caretModel.moveToOffset(element.textRange.startOffset)
        val target = singleTarget(element.firstChild ?: element)

        assertEquals(CpElements.FUNCTION_NAME, target.cpElementType())
        assertEquals(name, target.text)
        assertEquals(targetFile.containingFile.virtualFile.path, target.containingFile.virtualFile.path)

        val actionTargets = GotoDeclarationAction.findAllTargetElements(project, myFixture.editor, myFixture.caretOffset)
        assertEquals(1, actionTargets.size)
        assertEquals(target, actionTargets.single())
    }

    private fun configureLexerMemberCall() {
        myFixture.addFileToProject(
            "lexer/state.cp",
            """
            export module lexer.state;

            export struct diagnostic {}
            export struct lexer {
                diagnostics: vector<diagnostic>;
            }

            impl lexer {
                take_diagnostics(self&) -> vector<diagnostic>
                {
                    return diagnostics;
                }
            }
            """.trimIndent(),
        )
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            export module lexer;

            import lexer.state;

            export lex()
            {
                let state = lexer{};
                return state.<caret>take_diagnostics();
            }
            """.trimIndent(),
        )
    }
}
