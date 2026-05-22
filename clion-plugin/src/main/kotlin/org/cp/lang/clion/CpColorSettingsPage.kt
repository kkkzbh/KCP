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
            AttributesDescriptor("关键字//控制流", CpSyntaxHighlighter.CONTROL_KEYWORD),
            AttributesDescriptor("关键字//声明", CpSyntaxHighlighter.DECLARATION_KEYWORD),
            AttributesDescriptor("关键字//模块", CpSyntaxHighlighter.MODULE_KEYWORD),
            AttributesDescriptor("标识符", CpSyntaxHighlighter.IDENTIFIER),
            AttributesDescriptor("函数//声明", CpSyntaxHighlighter.FUNCTION_DECLARATION),
            AttributesDescriptor("函数//调用", CpSyntaxHighlighter.FUNCTION_CALL),
            AttributesDescriptor("函数//成员", CpSyntaxHighlighter.MEMBER_FUNCTION),
            AttributesDescriptor("函数//关联", CpSyntaxHighlighter.ASSOCIATED_FUNCTION),
            AttributesDescriptor("函数//构造", CpSyntaxHighlighter.CONSTRUCTOR),
            AttributesDescriptor("函数//析构", CpSyntaxHighlighter.DESTRUCTOR),
            AttributesDescriptor("函数//内建", CpSyntaxHighlighter.BUILTIN_FUNCTION),
            AttributesDescriptor("函数//类型标记", CpSyntaxHighlighter.FUNCTION_TYPE),
            AttributesDescriptor("变量//参数", CpSyntaxHighlighter.PARAMETER),
            AttributesDescriptor("变量//局部", CpSyntaxHighlighter.LOCAL_VARIABLE),
            AttributesDescriptor("变量//局部常量", CpSyntaxHighlighter.LOCAL_CONSTANT),
            AttributesDescriptor("变量//Lambda 捕获", CpSyntaxHighlighter.LAMBDA_CAPTURE),
            AttributesDescriptor("类型", CpSyntaxHighlighter.TYPE),
            AttributesDescriptor("类型//参数", CpSyntaxHighlighter.TYPE_PARAMETER),
            AttributesDescriptor("类型//别名", CpSyntaxHighlighter.TYPE_ALIAS),
            AttributesDescriptor("类型//关联", CpSyntaxHighlighter.ASSOCIATED_TYPE),
            AttributesDescriptor("概念", CpSyntaxHighlighter.CONCEPT),
            AttributesDescriptor("模块名", CpSyntaxHighlighter.MODULE_NAME),
            AttributesDescriptor("循环标签", CpSyntaxHighlighter.LABEL),
            AttributesDescriptor("字段", CpSyntaxHighlighter.FIELD),
            AttributesDescriptor("变体分支", CpSyntaxHighlighter.VARIANT_CASE),
            AttributesDescriptor("枚举项", CpSyntaxHighlighter.ENUM_CASE),
            AttributesDescriptor("字面量//布尔", CpSyntaxHighlighter.BOOLEAN),
            AttributesDescriptor("字面量//空指针", CpSyntaxHighlighter.NULL_LITERAL),
            AttributesDescriptor("字面量//数字", CpSyntaxHighlighter.NUMBER),
            AttributesDescriptor("字面量//字符串", CpSyntaxHighlighter.STRING),
            AttributesDescriptor("字面量//字符", CpSyntaxHighlighter.CHARACTER),
            AttributesDescriptor("注释//行注释", CpSyntaxHighlighter.LINE_COMMENT),
            AttributesDescriptor("注释//块注释", CpSyntaxHighlighter.BLOCK_COMMENT),
            AttributesDescriptor("运算符", CpSyntaxHighlighter.OPERATOR),
            AttributesDescriptor("标点", CpSyntaxHighlighter.PUNCTUATION),
            AttributesDescriptor("非法字符", CpSyntaxHighlighter.BAD_CHARACTER),
        )
    }
}
