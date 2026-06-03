package org.cp.lang.clion

import com.intellij.lang.LanguageParserDefinitions
import com.intellij.codeInsight.editorActions.TypedHandlerDelegate
import com.intellij.application.options.CodeStyle
import com.intellij.formatting.Block
import com.intellij.formatting.FormattingContext
import com.intellij.psi.codeStyle.CodeStyleSettingsCustomizable
import com.intellij.psi.codeStyle.CommonCodeStyleSettings
import com.intellij.psi.codeStyle.LanguageCodeStyleSettingsProvider
import com.intellij.testFramework.fixtures.BasePlatformTestCase
import org.junit.Assert.assertFalse
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.lang.reflect.Proxy

class CpEditorSupportTest {
    @Test
    fun smartBackspaceDeletesToPreviousIndentStop() {
        val text = "main() {\n       "
        val deletion = CpEditorIndent.smartBackspaceDeletion(text, text.length)

        assertEquals(13..15, deletion)
    }

    @Test
    fun smartBackspaceIgnoresNonIndentText() {
        val text = "main() {\n    let"

        assertNull(CpEditorIndent.smartBackspaceDeletion(text, text.length))
    }

    @Test
    fun enterAfterOpenBraceIndentsBlankLine() {
        val text = "main() {\n"

        val edit = CpEditorIndent.openBraceIndentEdit(text, text.length)

        assertEquals("main() {\n    ", applyEdit(text, edit))
        assertEquals("main() {\n    ".length, edit?.caretOffset)
    }

    @Test
    fun enterBetweenPairedBracesSplitsClosingBraceLine() {
        val text = "main() {\n}"
        val caretOffset = "main() {\n".length

        val edit = CpEditorIndent.openBraceIndentEdit(text, caretOffset)

        assertEquals(
            "main() {\n    \n}",
            applyEdit(text, edit),
        )
        assertEquals("main() {\n    ".length, edit?.caretOffset)
    }

    @Test
    fun editorIndentLogicRejectsOutOfScopeCaretPositions() {
        assertNull(CpEditorIndent.smartBackspaceDeletion("", 0))
        assertNull(CpEditorIndent.smartBackspaceDeletion("    ", 5))
        assertNull(CpEditorIndent.smartBackspaceDeletion("\n    ", 1))
        assertNull(CpEditorIndent.openBraceIndentEdit("main() {\n    let", "main() {\n    let".length))
        assertNull(CpEditorIndent.openBraceIndentEdit("main()\n", "main()\n".length))
        assertNull(CpEditorIndent.openBraceIndentEdit("main() {\n", -1))
    }

    @Test
    fun commenterBraceMatcherAndHighlighterFactoryExposePluginContracts() {
        val commenter = CpCommenter()
        assertEquals("//", commenter.lineCommentPrefix)
        assertEquals("/*", commenter.blockCommentPrefix)
        assertEquals("*/", commenter.blockCommentSuffix)
        assertNull(commenter.commentedBlockCommentPrefix)
        assertNull(commenter.commentedBlockCommentSuffix)

        val matcher = CpBraceMatcher()
        assertEquals(3, matcher.pairs.size)
        assertTrue(matcher.isPairedBracesAllowedBeforeType(CpTypes.L_BRACE, null))

        assertTrue(CpSyntaxHighlighterFactory().getSyntaxHighlighter(null, null) is CpSyntaxHighlighter)
        assertNotNull(CpQuoteHandler())
    }

    @Test
    fun colorSettingsPageDescribesAllCustomHighlightTags() {
        val page = CpColorSettingsPage()

        assertEquals("cp", page.displayName)
        assertNull(page.icon)
        assertTrue(page.highlighter is CpSyntaxHighlighter)
        assertTrue(page.demoText.contains("<typeParameter>T</typeParameter>"))
        assertTrue(page.demoText.contains("<variantCase>key</variantCase>"))
        assertTrue(page.additionalHighlightingTagToDescriptorMap.keys.containsAll(
            listOf(
                "functionDecl",
                "memberFunction",
                "typeParameter",
                "variantCase",
                "enumCase",
            ),
        ))
        assertTrue(page.attributeDescriptors.any { it.displayName == "函数//声明" })
        assertTrue(page.colorDescriptors.isEmpty())
    }

    @Test
    fun codeStyleProviderPublishesCpDefaultsAndVisibleOptionGroups() {
        val provider = CpCodeStyleSettingsProvider()
        val common = CommonCodeStyleSettings(CpLanguage)
        val indent = common.initIndentOptions()

        CpCodeStyleSettingsProvider::class.java
            .getDeclaredMethod(
                "customizeDefaults",
                CommonCodeStyleSettings::class.java,
                CommonCodeStyleSettings.IndentOptions::class.java,
            )
            .also { it.isAccessible = true }
            .invoke(provider, common, indent)

        assertEquals(CpLanguage, provider.language)
        assertTrue(provider.getCodeSample(LanguageCodeStyleSettingsProvider.SettingsType.SPACING_SETTINGS).contains("for(let i in iota"))
        assertEquals(CpEditorIndent.INDENT_SIZE, indent.INDENT_SIZE)
        assertFalse(indent.USE_TAB_CHARACTER)
        assertTrue(common.SPACE_AROUND_ASSIGNMENT_OPERATORS)
        assertEquals(CommonCodeStyleSettings.FORCE_BRACES_ALWAYS, common.IF_BRACE_FORCE)

        val requested = mutableMapOf<LanguageCodeStyleSettingsProvider.SettingsType, Set<String>>()
        for (settingsType in LanguageCodeStyleSettingsProvider.SettingsType.entries) {
            val shown = mutableSetOf<String>()
            provider.customizeSettings(capturingCustomizer(shown), settingsType)
            requested[settingsType] = shown
        }

        assertTrue(requested[LanguageCodeStyleSettingsProvider.SettingsType.INDENT_SETTINGS]!!.contains("INDENT_SIZE"))
        assertTrue(requested[LanguageCodeStyleSettingsProvider.SettingsType.SPACING_SETTINGS]!!.contains("SPACE_AFTER_COMMA"))
        assertTrue(requested[LanguageCodeStyleSettingsProvider.SettingsType.WRAPPING_AND_BRACES_SETTINGS]!!.contains("CALL_PARAMETERS_WRAP"))
    }

    private fun applyEdit(text: String, edit: CpEditorIndent.OpenBraceIndentEdit?): String {
        if (edit == null) {
            return text
        }

        val builder = StringBuilder(text)
        builder.replace(edit.replaceStartOffset, edit.replaceEndOffset, edit.replacement)
        edit.insertOffset?.let { builder.insert(it, edit.insertedText) }
        return builder.toString()
    }

    private fun capturingCustomizer(shown: MutableSet<String>): CodeStyleSettingsCustomizable =
        Proxy.newProxyInstance(
            CodeStyleSettingsCustomizable::class.java.classLoader,
            arrayOf(CodeStyleSettingsCustomizable::class.java),
        ) { _, method, args ->
            if (method.name == "showStandardOptions") {
                args.orEmpty()
                    .asSequence()
                    .flatMap { argument ->
                        when (argument) {
                            is Array<*> -> argument.asSequence()
                            else -> sequenceOf(argument)
                        }
                    }
                    .filterIsInstance<String>()
                    .forEach(shown::add)
            }
            null
        } as CodeStyleSettingsCustomizable
}

class CpQuoteTypedHandlerTest : BasePlatformTestCase() {
    private val parserDefinition = CpParserDefinition()

    override fun setUp() {
        super.setUp()
        LanguageParserDefinitions.INSTANCE.addExplicitExtension(CpLanguage, parserDefinition)
        TypedHandlerDelegate.EP_NAME.point.registerExtension(CpQuoteTypedHandler(), testRootDisposable)
    }

    override fun tearDown() {
        try {
            LanguageParserDefinitions.INSTANCE.removeExplicitExtension(CpLanguage, parserDefinition)
        } finally {
            super.tearDown()
        }
    }

    fun testDoubleQuoteTypingInsertsPair() {
        myFixture.configureByText(CpFileType.INSTANCE, "main() { let text = <caret>; }")

        myFixture.type("\"")

        myFixture.checkResult("main() { let text = \"<caret>\"; }")
    }

    fun testSingleQuoteTypingInsertsPair() {
        myFixture.configureByText(CpFileType.INSTANCE, "main() { let ch = <caret>; }")

        myFixture.type("'")

        myFixture.checkResult("main() { let ch = '<caret>'; }")
    }

    fun testSmartSingleQuoteTypingInsertsPair() {
        myFixture.configureByText(CpFileType.INSTANCE, "main() { let ch = <caret>; }")

        myFixture.type("‘")

        myFixture.checkResult("main() { let ch = ‘<caret>’; }")
    }

    fun testTypingQuoteBeforeClosingQuoteSkipsOverIt() {
        myFixture.configureByText(CpFileType.INSTANCE, "main() { let text = \"<caret>\"; }")

        myFixture.type("\"")

        myFixture.checkResult("main() { let text = \"\"<caret>; }")
    }

    @Test
    fun quotePairLogicIgnoresEscapedPosition() {
        assertNull(CpQuotePairs.typingEdit("\\", 1, '"'))
    }

    @Test
    fun quotePairLogicCoversBoundaryAndUnicodeCases() {
        assertNull(CpQuotePairs.typingEdit("abc", -1, '"'))
        assertNull(CpQuotePairs.typingEdit("abc", 4, '"'))
        assertNull(CpQuotePairs.typingEdit("abc", 1, '"'))
        assertNull(CpQuotePairs.typingEdit("abc", 3, '`'))
        assertEquals(CpQuotePairs.SkipClosingQuote, CpQuotePairs.typingEdit("\"", 0, '"'))
        assertEquals(CpQuotePairs.InsertPair('“', '”'), CpQuotePairs.typingEdit("", 0, '“'))
        assertEquals(CpQuotePairs.InsertPair('"', '"'), CpQuotePairs.typingEdit("\\\\", 2, '"'))
    }
}

class CpFormattingTest : BasePlatformTestCase() {
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

    fun testFormattingModelBuildsNestedBlocksAndSpacing() {
        myFixture.configureByText(CpFileType.INSTANCE, "main(){let x=1+2;if(x>2){return x;}else{return 0;}}")

        val model = CpFormattingModelBuilder().createModel(
            FormattingContext.create(myFixture.file, CodeStyle.getSettings(myFixture.file)),
        )
        val blocks = collectBlocks(model.rootBlock)
        val parent = blocks.first { it.subBlocks.size >= 2 }

        assertTrue(blocks.size > 5)
        assertNotNull(parent.getSpacing(parent.subBlocks[0], parent.subBlocks[1]))
        assertNotNull(parent.getChildAttributes(0))
    }

    private fun collectBlocks(block: Block): List<Block> =
        listOf(block) + block.subBlocks.flatMap(::collectBlocks)
}
