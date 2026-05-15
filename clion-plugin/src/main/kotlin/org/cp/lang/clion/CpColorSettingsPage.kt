package org.cp.lang.clion

import com.intellij.openapi.editor.colors.TextAttributesKey
import com.intellij.openapi.fileTypes.SyntaxHighlighter
import com.intellij.openapi.options.colors.AttributesDescriptor
import com.intellij.openapi.options.colors.ColorDescriptor
import com.intellij.openapi.options.colors.ColorSettingsPage
import javax.swing.Icon

class CpColorSettingsPage : ColorSettingsPage {
    override fun getIcon(): Icon? = null

    override fun getHighlighter(): SyntaxHighlighter = CpSyntaxHighlighter()

    override fun getDemoText(): String =
        """
        export module <module>math</module>;

        import <module>std.io</module>;

        export <functionDecl>add</functionDecl>(<parameter>x</parameter>: <type>i32</type>, <parameter>y</parameter>: <type>i32</type>) -> <type>i32</type>
        {
            let <local>x2</local> = <functionCall>add</functionCall>(<local>x</local>, <local>y</local>);
            return <local>x2</local> + 1;
        }
        """.trimIndent()

    override fun getAdditionalHighlightingTagToDescriptorMap(): Map<String, TextAttributesKey> = mapOf(
        "functionDecl" to CpSyntaxHighlighter.FUNCTION_DECLARATION,
        "functionCall" to CpSyntaxHighlighter.FUNCTION_CALL,
        "parameter" to CpSyntaxHighlighter.PARAMETER,
        "local" to CpSyntaxHighlighter.LOCAL_VARIABLE,
        "type" to CpSyntaxHighlighter.TYPE,
        "module" to CpSyntaxHighlighter.MODULE_NAME,
        "label" to CpSyntaxHighlighter.LABEL,
    )

    override fun getAttributeDescriptors(): Array<AttributesDescriptor> = DESCRIPTORS

    override fun getColorDescriptors(): Array<ColorDescriptor> = ColorDescriptor.EMPTY_ARRAY

    override fun getDisplayName(): String = "cp"

    private companion object {
        val DESCRIPTORS: Array<AttributesDescriptor> = arrayOf(
            AttributesDescriptor("Keyword//Control", CpSyntaxHighlighter.CONTROL_KEYWORD),
            AttributesDescriptor("Keyword//Declaration", CpSyntaxHighlighter.DECLARATION_KEYWORD),
            AttributesDescriptor("Keyword//Module", CpSyntaxHighlighter.MODULE_KEYWORD),
            AttributesDescriptor("Identifier", CpSyntaxHighlighter.IDENTIFIER),
            AttributesDescriptor("Function//Declaration", CpSyntaxHighlighter.FUNCTION_DECLARATION),
            AttributesDescriptor("Function//Call", CpSyntaxHighlighter.FUNCTION_CALL),
            AttributesDescriptor("Variable//Parameter", CpSyntaxHighlighter.PARAMETER),
            AttributesDescriptor("Variable//Local", CpSyntaxHighlighter.LOCAL_VARIABLE),
            AttributesDescriptor("Type", CpSyntaxHighlighter.TYPE),
            AttributesDescriptor("Module name", CpSyntaxHighlighter.MODULE_NAME),
            AttributesDescriptor("Loop label", CpSyntaxHighlighter.LABEL),
            AttributesDescriptor("Literal//Boolean", CpSyntaxHighlighter.BOOLEAN),
            AttributesDescriptor("Literal//Number", CpSyntaxHighlighter.NUMBER),
            AttributesDescriptor("Literal//String", CpSyntaxHighlighter.STRING),
            AttributesDescriptor("Literal//Character", CpSyntaxHighlighter.CHARACTER),
            AttributesDescriptor("Comment//Line", CpSyntaxHighlighter.LINE_COMMENT),
            AttributesDescriptor("Comment//Block", CpSyntaxHighlighter.BLOCK_COMMENT),
            AttributesDescriptor("Operator", CpSyntaxHighlighter.OPERATOR),
            AttributesDescriptor("Punctuation", CpSyntaxHighlighter.PUNCTUATION),
            AttributesDescriptor("Bad character", CpSyntaxHighlighter.BAD_CHARACTER),
        )
    }
}
