package org.cp.lang.clion

import com.intellij.codeInsight.editorActions.BackspaceHandlerDelegate
import com.intellij.codeInsight.editorActions.SimpleTokenSetQuoteHandler
import com.intellij.codeInsight.editorActions.enter.EnterHandlerDelegate
import com.intellij.formatting.Block
import com.intellij.formatting.ChildAttributes
import com.intellij.formatting.FormattingContext
import com.intellij.formatting.FormattingModel
import com.intellij.formatting.FormattingModelBuilder
import com.intellij.formatting.FormattingModelProvider
import com.intellij.formatting.Indent
import com.intellij.formatting.Spacing
import com.intellij.formatting.SpacingBuilder
import com.intellij.formatting.Wrap
import com.intellij.lang.ASTNode
import com.intellij.lang.BracePair
import com.intellij.lang.Commenter
import com.intellij.lang.PairedBraceMatcher
import com.intellij.openapi.actionSystem.DataContext
import com.intellij.openapi.editor.Document
import com.intellij.openapi.editor.Editor
import com.intellij.openapi.editor.actionSystem.EditorActionHandler
import com.intellij.openapi.util.Ref
import com.intellij.openapi.util.TextRange
import com.intellij.psi.PsiFile
import com.intellij.psi.TokenType
import com.intellij.psi.codeStyle.CodeStyleSettingsCustomizable
import com.intellij.psi.codeStyle.CommonCodeStyleSettings
import com.intellij.psi.codeStyle.LanguageCodeStyleSettingsProvider
import com.intellij.psi.tree.IElementType
import com.intellij.psi.tree.TokenSet

class CpCommenter : Commenter {
    override fun getLineCommentPrefix(): String = "//"

    override fun getBlockCommentPrefix(): String = "/*"

    override fun getBlockCommentSuffix(): String = "*/"

    override fun getCommentedBlockCommentPrefix(): String? = null

    override fun getCommentedBlockCommentSuffix(): String? = null
}

class CpBraceMatcher : PairedBraceMatcher {
    override fun getPairs(): Array<BracePair> = PAIRS

    override fun isPairedBracesAllowedBeforeType(lbraceType: IElementType, contextType: IElementType?): Boolean = true

    override fun getCodeConstructStart(file: PsiFile, openingBraceOffset: Int): Int = openingBraceOffset

    private companion object {
        private val PAIRS = arrayOf(
            BracePair(CpTypes.L_PAREN, CpTypes.R_PAREN, false),
            BracePair(CpTypes.L_BRACE, CpTypes.R_BRACE, true),
            BracePair(CpTypes.L_BRACKET, CpTypes.R_BRACKET, false),
        )
    }
}

class CpQuoteHandler : SimpleTokenSetQuoteHandler(CpTypes.STRING_TOKENS)

class CpCodeStyleSettingsProvider : LanguageCodeStyleSettingsProvider() {
    override fun getLanguage() = CpLanguage

    override fun getCodeSample(settingsType: SettingsType): String =
        """
        main() {
            let sum = 0;
            for(let i in iota(0, 5)) {
                sum = sum + i;
            }
            return sum;
        }
        """.trimIndent()

    override fun customizeSettings(consumer: CodeStyleSettingsCustomizable, settingsType: SettingsType) {
        when (settingsType) {
            SettingsType.INDENT_SETTINGS -> {
                consumer.showStandardOptions(
                    "INDENT_SIZE",
                    "TAB_SIZE",
                    "CONTINUATION_INDENT_SIZE",
                    "USE_TAB_CHARACTER",
                    "SMART_TABS",
                    "KEEP_INDENTS_ON_EMPTY_LINES",
                )
            }
            SettingsType.SPACING_SETTINGS -> {
                consumer.showStandardOptions(
                    "SPACE_AROUND_ASSIGNMENT_OPERATORS",
                    "SPACE_AROUND_LOGICAL_OPERATORS",
                    "SPACE_AROUND_EQUALITY_OPERATORS",
                    "SPACE_AROUND_RELATIONAL_OPERATORS",
                    "SPACE_AROUND_ADDITIVE_OPERATORS",
                    "SPACE_AROUND_MULTIPLICATIVE_OPERATORS",
                    "SPACE_AFTER_COMMA",
                    "SPACE_BEFORE_COMMA",
                    "SPACE_AFTER_COLON",
                    "SPACE_BEFORE_COLON",
                    "SPACE_WITHIN_PARENTHESES",
                    "SPACE_WITHIN_BRACKETS",
                    "SPACE_BEFORE_IF_PARENTHESES",
                    "SPACE_BEFORE_WHILE_PARENTHESES",
                    "SPACE_BEFORE_FOR_PARENTHESES",
                    "SPACE_BEFORE_METHOD_LBRACE",
                    "SPACE_BEFORE_IF_LBRACE",
                    "SPACE_BEFORE_WHILE_LBRACE",
                    "SPACE_BEFORE_FOR_LBRACE",
                )
            }
            SettingsType.WRAPPING_AND_BRACES_SETTINGS -> {
                consumer.showStandardOptions(
                    "KEEP_LINE_BREAKS",
                    "KEEP_BLANK_LINES_IN_DECLARATIONS",
                    "KEEP_BLANK_LINES_IN_CODE",
                    "BRACE_STYLE",
                    "METHOD_BRACE_STYLE",
                    "ELSE_ON_NEW_LINE",
                    "WHILE_ON_NEW_LINE",
                    "ALIGN_MULTILINE_PARAMETERS",
                    "ALIGN_MULTILINE_PARAMETERS_IN_CALLS",
                    "ALIGN_MULTILINE_FOR",
                    "ALIGN_MULTILINE_BINARY_OPERATION",
                    "BINARY_OPERATION_SIGN_ON_NEXT_LINE",
                    "CALL_PARAMETERS_WRAP",
                    "METHOD_PARAMETERS_WRAP",
                    "IF_BRACE_FORCE",
                    "WHILE_BRACE_FORCE",
                    "FOR_BRACE_FORCE",
                )
            }
            else -> Unit
        }
    }

    override fun customizeDefaults(
        commonSettings: CommonCodeStyleSettings,
        indentOptions: CommonCodeStyleSettings.IndentOptions,
    ) {
        indentOptions.INDENT_SIZE = CpEditorIndent.INDENT_SIZE
        indentOptions.TAB_SIZE = CpEditorIndent.INDENT_SIZE
        indentOptions.CONTINUATION_INDENT_SIZE = CpEditorIndent.INDENT_SIZE
        indentOptions.USE_TAB_CHARACTER = false
        indentOptions.SMART_TABS = false
        indentOptions.KEEP_INDENTS_ON_EMPTY_LINES = false

        commonSettings.KEEP_LINE_BREAKS = true
        commonSettings.KEEP_BLANK_LINES_IN_DECLARATIONS = 1
        commonSettings.KEEP_BLANK_LINES_IN_CODE = 1
        commonSettings.BRACE_STYLE = CommonCodeStyleSettings.END_OF_LINE
        commonSettings.CLASS_BRACE_STYLE = CommonCodeStyleSettings.END_OF_LINE
        commonSettings.METHOD_BRACE_STYLE = CommonCodeStyleSettings.END_OF_LINE
        commonSettings.LAMBDA_BRACE_STYLE = CommonCodeStyleSettings.END_OF_LINE
        commonSettings.ELSE_ON_NEW_LINE = false
        commonSettings.WHILE_ON_NEW_LINE = false
        commonSettings.SPECIAL_ELSE_IF_TREATMENT = true
        commonSettings.ALIGN_MULTILINE_PARAMETERS = true
        commonSettings.ALIGN_MULTILINE_PARAMETERS_IN_CALLS = true
        commonSettings.ALIGN_MULTILINE_FOR = true
        commonSettings.ALIGN_MULTILINE_BINARY_OPERATION = true
        commonSettings.BINARY_OPERATION_SIGN_ON_NEXT_LINE = true
        commonSettings.CALL_PARAMETERS_WRAP = CommonCodeStyleSettings.WRAP_AS_NEEDED
        commonSettings.METHOD_PARAMETERS_WRAP = CommonCodeStyleSettings.WRAP_AS_NEEDED
        commonSettings.SPACE_AROUND_ASSIGNMENT_OPERATORS = true
        commonSettings.SPACE_AROUND_LOGICAL_OPERATORS = true
        commonSettings.SPACE_AROUND_EQUALITY_OPERATORS = true
        commonSettings.SPACE_AROUND_RELATIONAL_OPERATORS = true
        commonSettings.SPACE_AROUND_ADDITIVE_OPERATORS = true
        commonSettings.SPACE_AROUND_MULTIPLICATIVE_OPERATORS = true
        commonSettings.SPACE_AFTER_COMMA = true
        commonSettings.SPACE_BEFORE_COMMA = false
        commonSettings.SPACE_AFTER_COLON = true
        commonSettings.SPACE_BEFORE_COLON = false
        commonSettings.SPACE_WITHIN_PARENTHESES = false
        commonSettings.SPACE_WITHIN_BRACKETS = false
        commonSettings.SPACE_BEFORE_IF_PARENTHESES = false
        commonSettings.SPACE_BEFORE_WHILE_PARENTHESES = false
        commonSettings.SPACE_BEFORE_FOR_PARENTHESES = false
        commonSettings.SPACE_BEFORE_METHOD_LBRACE = true
        commonSettings.SPACE_BEFORE_IF_LBRACE = true
        commonSettings.SPACE_BEFORE_WHILE_LBRACE = true
        commonSettings.SPACE_BEFORE_FOR_LBRACE = true
        commonSettings.IF_BRACE_FORCE = CommonCodeStyleSettings.FORCE_BRACES_ALWAYS
        commonSettings.WHILE_BRACE_FORCE = CommonCodeStyleSettings.FORCE_BRACES_ALWAYS
        commonSettings.FOR_BRACE_FORCE = CommonCodeStyleSettings.FORCE_BRACES_ALWAYS
    }
}

class CpFormattingModelBuilder : FormattingModelBuilder {
    override fun createModel(formattingContext: FormattingContext): FormattingModel {
        val block = CpFormatBlock(
            node = formattingContext.node,
            spacingBuilder = CpFormatSpacing.create(formattingContext.codeStyleSettings),
            indent = Indent.getNoneIndent(),
        )

        return FormattingModelProvider.createFormattingModelForPsiFile(
            formattingContext.containingFile,
            block,
            formattingContext.codeStyleSettings,
        )
    }
}

private class CpFormatBlock(
    private val node: ASTNode,
    private val spacingBuilder: SpacingBuilder,
    private val indent: Indent,
) : Block {
    override fun getTextRange(): TextRange = node.textRange

    override fun getSubBlocks(): List<Block> =
        node.getChildren(null)
            .filter { it.elementType != TokenType.WHITE_SPACE }
            .map {
                CpFormatBlock(
                    node = it,
                    spacingBuilder = spacingBuilder,
                    indent = childIndent(it),
                )
            }

    override fun getWrap(): Wrap? = null

    override fun getIndent(): Indent = indent

    override fun getAlignment() = null

    override fun getSpacing(child1: Block?, child2: Block): Spacing? =
        structuralSpacing(child1, child2)
            ?: spacingBuilder.getSpacing(this, child1, child2)
            ?: Spacing.createSpacing(0, 1, 0, true, 0)

    override fun getChildAttributes(newChildIndex: Int): ChildAttributes =
        ChildAttributes(
            if (node.elementType in CpFormatSpacing.BRACE_CONTAINERS) {
                Indent.getNormalIndent()
            } else {
                Indent.getNoneIndent()
            },
            null,
        )

    override fun isIncomplete(): Boolean = false

    override fun isLeaf(): Boolean = node.firstChildNode == null

    private fun childIndent(child: ASTNode): Indent =
        if (node.elementType in CpFormatSpacing.BRACE_CONTAINERS && child.elementType !in CpFormatSpacing.BRACE_TOKENS) {
            Indent.getNormalIndent()
        } else {
            Indent.getNoneIndent()
        }

    private fun structuralSpacing(child1: Block?, child2: Block): Spacing? {
        if (node.elementType !in CpFormatSpacing.BRACE_CONTAINERS) {
            return null
        }

        val left = child1 as? CpFormatBlock
        val right = child2 as? CpFormatBlock
        return when {
            left?.node?.elementType == CpTypes.L_BRACE -> CpFormatSpacing.LINE_BREAK
            right?.node?.elementType == CpTypes.R_BRACE -> CpFormatSpacing.LINE_BREAK
            left?.node?.text?.trimEnd()?.endsWith(";") == true -> CpFormatSpacing.LINE_BREAK
            else -> null
        }
    }
}

private object CpFormatSpacing {
    val LINE_BREAK: Spacing = Spacing.createSpacing(0, 0, 1, true, 0)

    val BRACE_CONTAINERS: TokenSet = TokenSet.create(
        CpElements.BLOCK,
        CpElements.BLOCK_EXPRESSION,
        CpElements.STRUCT_DECLARATION,
        CpElements.ENUM_DECLARATION,
        CpElements.VARIANT_DECLARATION,
        CpElements.CONCEPT_DECLARATION,
        CpElements.IMPL_BLOCK,
        CpElements.STRUCT_INIT_EXPRESSION,
        CpElements.MATCH_EXPRESSION,
        CpElements.ARRAY_LITERAL,
        CpElements.TUPLE_LITERAL,
    )

    val BRACE_TOKENS: TokenSet = TokenSet.create(CpTypes.L_BRACE, CpTypes.R_BRACE)

    private val ASSIGNMENT_OPERATORS: TokenSet = TokenSet.create(
        CpTypes.EQUAL,
        CpTypes.PLUS_EQUAL,
        CpTypes.MINUS_EQUAL,
        CpTypes.STAR_EQUAL,
        CpTypes.SLASH_EQUAL,
        CpTypes.PERCENT_EQUAL,
        CpTypes.AMP_EQUAL,
        CpTypes.PIPE_EQUAL,
        CpTypes.CARET_EQUAL,
        CpTypes.LESS_LESS_EQUAL,
        CpTypes.GREATER_GREATER_EQUAL,
    )

    private val BINARY_OPERATORS: TokenSet = TokenSet.create(
        CpTypes.KW_AS,
        CpTypes.KW_AND,
        CpTypes.KW_OR,
        CpTypes.PLUS,
        CpTypes.MINUS,
        CpTypes.STAR,
        CpTypes.SLASH,
        CpTypes.PERCENT,
        CpTypes.EQUAL_EQUAL,
        CpTypes.BANG_EQUAL,
        CpTypes.LESS,
        CpTypes.LESS_EQUAL,
        CpTypes.GREATER,
        CpTypes.GREATER_EQUAL,
        CpTypes.AMP,
        CpTypes.PIPE,
        CpTypes.CARET,
        CpTypes.LESS_LESS,
        CpTypes.GREATER_GREATER,
    )

    fun create(settings: com.intellij.psi.codeStyle.CodeStyleSettings): SpacingBuilder =
        SpacingBuilder(settings, CpLanguage)
            .between(CpTypes.KW_IF, CpTypes.L_PAREN).none()
            .between(CpTypes.KW_WHILE, CpTypes.L_PAREN).none()
            .between(CpTypes.KW_FOR, CpTypes.L_PAREN).none()
            .before(CpTypes.L_BRACE).spaces(1)
            .afterInside(CpTypes.L_BRACE, BRACE_CONTAINERS).lineBreakInCode()
            .beforeInside(CpTypes.R_BRACE, BRACE_CONTAINERS).lineBreakInCode()
            .afterInside(CpTypes.SEMICOLON, BRACE_CONTAINERS).lineBreakInCode()
            .before(CpTypes.COMMA).none()
            .after(CpTypes.COMMA).spaces(1)
            .before(CpTypes.SEMICOLON).none()
            .before(CpTypes.COLON).none()
            .after(CpTypes.COLON).spaces(1)
            .around(CpTypes.ARROW).spaces(1)
            .aroundInside(ASSIGNMENT_OPERATORS, CpElements.ASSIGNMENT_EXPRESSION).spaces(1)
            .aroundInside(BINARY_OPERATORS, CpElements.BINARY_EXPRESSION).spaces(1)
            .aroundInside(CpTypes.KW_AS, CpElements.CAST_EXPRESSION).spaces(1)
            .afterInside(CpTypes.KW_NOT, CpElements.UNARY_EXPRESSION).spaces(1)
            .withinPair(CpTypes.L_PAREN, CpTypes.R_PAREN).none()
            .withinPair(CpTypes.L_BRACKET, CpTypes.R_BRACKET).none()
            .between(CpTypes.R_BRACE, CpTypes.KW_ELSE).spaces(1)
}

class CpBackspaceHandler : BackspaceHandlerDelegate() {
    override fun beforeCharDeleted(c: Char, file: PsiFile, editor: Editor) = Unit

    override fun charDeleted(c: Char, file: PsiFile, editor: Editor): Boolean {
        if (file.language != CpLanguage || c != ' ') {
            return false
        }

        val caretOffset = editor.caretModel.offset
        val deletion = CpEditorIndent.smartBackspaceDeletion(editor.document.charsSequence, caretOffset)
            ?: return false

        editor.document.deleteString(deletion.first, deletion.last + 1)
        return true
    }
}

class CpEnterHandler : EnterHandlerDelegate {
    override fun preprocessEnter(
        file: PsiFile,
        editor: Editor,
        caretOffset: Ref<Int>,
        caretAdvance: Ref<Int>,
        dataContext: DataContext,
        originalHandler: EditorActionHandler?,
    ): EnterHandlerDelegate.Result = EnterHandlerDelegate.Result.Continue

    override fun postProcessEnter(file: PsiFile, editor: Editor, dataContext: DataContext): EnterHandlerDelegate.Result {
        if (file.language != CpLanguage) {
            return EnterHandlerDelegate.Result.Continue
        }

        CpEditorIndent.reindentAfterOpenBrace(editor.document, editor.caretModel.offset)
            ?.let { editor.caretModel.moveToOffset(it) }

        return EnterHandlerDelegate.Result.Continue
    }
}

object CpEditorIndent {
    const val INDENT_SIZE = 4

    data class OpenBraceIndentEdit(
        val replaceStartOffset: Int,
        val replaceEndOffset: Int,
        val replacement: String,
        val insertOffset: Int?,
        val insertedText: String,
        val caretOffset: Int,
    )

    fun smartBackspaceDeletion(text: CharSequence, caretOffset: Int): IntRange? {
        if (caretOffset <= 0 || caretOffset > text.length) {
            return null
        }

        val lineStart = text.lineStartOffset(caretOffset)
        if (caretOffset == lineStart || text.subSequence(lineStart, caretOffset).any { it != ' ' }) {
            return null
        }

        val extraSpaces = caretOffset.columnFrom(lineStart) % INDENT_SIZE
        if (extraSpaces == 0) {
            return null
        }

        val deleteCount = minOf(extraSpaces, caretOffset - lineStart)
        return (caretOffset - deleteCount)..<caretOffset
    }

    fun reindentAfterOpenBrace(document: Document, caretOffset: Int): Int? {
        val edit = openBraceIndentEdit(document.charsSequence, caretOffset)
            ?: return null

        document.replaceString(edit.replaceStartOffset, edit.replaceEndOffset, edit.replacement)
        edit.insertOffset?.let { document.insertString(it, edit.insertedText) }
        return edit.caretOffset
    }

    fun openBraceIndentEdit(text: CharSequence, caretOffset: Int): OpenBraceIndentEdit? {
        if (caretOffset !in 0..text.length) {
            return null
        }

        val lineStart = text.lineStartOffset(caretOffset)
        if (lineStart == 0) {
            return null
        }

        val previousLineEnd = lineStart - 1
        val previousLineStart = text.lineStartOffset(previousLineEnd)
        val previousLine = text.subSequence(previousLineStart, previousLineEnd).toString()
        if (!previousLine.trimEnd().endsWith("{")) {
            return null
        }

        val baseIndent = previousLine.takeWhile { it == ' ' }
        val innerIndent = baseIndent + " ".repeat(INDENT_SIZE)
        val lineEnd = text.lineEndOffset(caretOffset)
        val beforeCaret = text.subSequence(lineStart, caretOffset).toString()
        if (beforeCaret.any { it != ' ' }) {
            return null
        }

        val afterCaret = text.subSequence(caretOffset, lineEnd).toString()
        val newCaretOffset = lineStart + innerIndent.length
        val insertedText = if (afterCaret.trimStart().startsWith("}")) "\n$baseIndent" else ""

        return OpenBraceIndentEdit(
            replaceStartOffset = lineStart,
            replaceEndOffset = caretOffset,
            replacement = innerIndent,
            insertOffset = insertedText.takeIf { it.isNotEmpty() }?.let { newCaretOffset },
            insertedText = insertedText,
            caretOffset = newCaretOffset,
        )
    }

    private fun CharSequence.lineStartOffset(offset: Int): Int {
        var current = offset
        while (current > 0 && this[current - 1] != '\n') {
            current -= 1
        }
        return current
    }

    private fun CharSequence.lineEndOffset(offset: Int): Int {
        var current = offset
        while (current < length && this[current] != '\n') {
            current += 1
        }
        return current
    }

    private fun Int.columnFrom(lineStart: Int): Int = this - lineStart
}
