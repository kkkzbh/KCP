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

        concept <concept>measurable</concept> {
            <conceptFunction>size</conceptFunction>(<parameter>self</parameter> const&) -> <type>i32</type>;
        }

        struct <type>box</type><<typeParameter>T</typeParameter>> {
            <field>value</field>: <typeParameter>T</typeParameter>;
        }

        impl <type>box</type><<typeParameter>T</typeParameter>> {
            <associatedFunction>box</associatedFunction>(<parameter>value</parameter>: <typeParameter>T</typeParameter>)
            {
                return <type>box</type>{ .<field>value</field> = <parameter>value</parameter> };
            }

            <memberFunction>get</memberFunction>(<parameter>self</parameter> const&) -> <typeParameter>T</typeParameter>
            {
                return <field>value</field>;
            }

            type <associatedType>item</associatedType> = <typeParameter>T</typeParameter>;
        }

        impl <concept>measurable</concept> for <type>box</type><<type>i32</type>> {
            <memberFunction>size</memberFunction>(<parameter>self</parameter> const&) -> <type>i32</type>
            {
                return 1;
            }
        }

        export <functionDecl>add</functionDecl>(<parameter>x</parameter>: <type>i32</type>, <parameter>y</parameter>: <type>i32</type>) -> <type>i32</type>
        {
            const <constant>scale</constant>: <type>i32</type> = 2;
            let <local>x2</local> = <functionCall>add</functionCall>(<parameter>x</parameter>, <parameter>y</parameter>);
            let point = vec2{ .<field>x</field> = 1, .<field>y</field> = 2 };
            let area = point.<memberFunction>length</memberFunction>();
            let event = <type>event</type>::<variantCase>key</variantCase>('a');
            let missing = <nullLiteral>nullptr</nullLiteral>;
            let storage = <builtinFunction>alloc</builtinFunction><<type>i32</type>>(1);
            let rounded = <type>i32</type>(1.5);
            let callback: <functionType>f</functionType>(<type>i32</type>) -> <type>i32</type> =
                <functionType>f</functionType>(<parameter>value</parameter>) => <parameter>value</parameter> + <constant>scale</constant>;
            let score = match event {
                .<variantCase>key</variantCase>(<parameter>code</parameter>) => <parameter>code</parameter>,
                .<variantCase>quit</variantCase> => 0,
            };
            <builtinFunction>free</builtinFunction>(storage);
            return <local>x2</local> + <local>area</local> + rounded + callback(1);
        }
        """.trimIndent()

    override fun getAdditionalHighlightingTagToDescriptorMap(): Map<String, TextAttributesKey> = mapOf(
        "functionDecl" to CpSyntaxHighlighter.FUNCTION_DECLARATION,
        "functionCall" to CpSyntaxHighlighter.FUNCTION_CALL,
        "memberFunction" to CpSyntaxHighlighter.MEMBER_FUNCTION,
        "associatedFunction" to CpSyntaxHighlighter.ASSOCIATED_FUNCTION,
        "conceptFunction" to CpSyntaxHighlighter.FUNCTION_DECLARATION,
        "builtinFunction" to CpSyntaxHighlighter.BUILTIN_FUNCTION,
        "functionType" to CpSyntaxHighlighter.FUNCTION_TYPE,
        "parameter" to CpSyntaxHighlighter.PARAMETER,
        "local" to CpSyntaxHighlighter.LOCAL_VARIABLE,
        "constant" to CpSyntaxHighlighter.LOCAL_CONSTANT,
        "nullLiteral" to CpSyntaxHighlighter.NULL_LITERAL,
        "type" to CpSyntaxHighlighter.TYPE,
        "typeParameter" to CpSyntaxHighlighter.TYPE_PARAMETER,
        "associatedType" to CpSyntaxHighlighter.ASSOCIATED_TYPE,
        "concept" to CpSyntaxHighlighter.CONCEPT,
        "selfType" to CpSyntaxHighlighter.TYPE_PARAMETER,
        "module" to CpSyntaxHighlighter.MODULE_NAME,
        "label" to CpSyntaxHighlighter.LABEL,
        "field" to CpSyntaxHighlighter.FIELD,
        "variantCase" to CpSyntaxHighlighter.VARIANT_CASE,
        "enumCase" to CpSyntaxHighlighter.ENUM_CASE,
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
            AttributesDescriptor("Function//Member", CpSyntaxHighlighter.MEMBER_FUNCTION),
            AttributesDescriptor("Function//Associated", CpSyntaxHighlighter.ASSOCIATED_FUNCTION),
            AttributesDescriptor("Function//Constructor", CpSyntaxHighlighter.CONSTRUCTOR),
            AttributesDescriptor("Function//Destructor", CpSyntaxHighlighter.DESTRUCTOR),
            AttributesDescriptor("Function//Builtin", CpSyntaxHighlighter.BUILTIN_FUNCTION),
            AttributesDescriptor("Function//Type marker", CpSyntaxHighlighter.FUNCTION_TYPE),
            AttributesDescriptor("Variable//Parameter", CpSyntaxHighlighter.PARAMETER),
            AttributesDescriptor("Variable//Local", CpSyntaxHighlighter.LOCAL_VARIABLE),
            AttributesDescriptor("Variable//Local constant", CpSyntaxHighlighter.LOCAL_CONSTANT),
            AttributesDescriptor("Variable//Lambda capture", CpSyntaxHighlighter.LAMBDA_CAPTURE),
            AttributesDescriptor("Type", CpSyntaxHighlighter.TYPE),
            AttributesDescriptor("Type//Parameter", CpSyntaxHighlighter.TYPE_PARAMETER),
            AttributesDescriptor("Type//Alias", CpSyntaxHighlighter.TYPE_ALIAS),
            AttributesDescriptor("Type//Associated", CpSyntaxHighlighter.ASSOCIATED_TYPE),
            AttributesDescriptor("Concept", CpSyntaxHighlighter.CONCEPT),
            AttributesDescriptor("Module name", CpSyntaxHighlighter.MODULE_NAME),
            AttributesDescriptor("Loop label", CpSyntaxHighlighter.LABEL),
            AttributesDescriptor("Field", CpSyntaxHighlighter.FIELD),
            AttributesDescriptor("Variant case", CpSyntaxHighlighter.VARIANT_CASE),
            AttributesDescriptor("Enum case", CpSyntaxHighlighter.ENUM_CASE),
            AttributesDescriptor("Literal//Boolean", CpSyntaxHighlighter.BOOLEAN),
            AttributesDescriptor("Literal//Null", CpSyntaxHighlighter.NULL_LITERAL),
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
