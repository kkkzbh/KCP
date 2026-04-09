package org.cp.lang.clion

import com.intellij.extapi.psi.ASTWrapperPsiElement
import com.intellij.extapi.psi.PsiFileBase
import com.intellij.lang.ASTNode
import com.intellij.lang.ParserDefinition
import com.intellij.lang.PsiParser
import com.intellij.lang.PsiBuilder
import com.intellij.lexer.Lexer
import com.intellij.openapi.project.Project
import com.intellij.psi.FileViewProvider
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiFile
import com.intellij.psi.TokenType
import com.intellij.psi.tree.IFileElementType
import com.intellij.psi.tree.TokenSet

class CpParserDefinition : ParserDefinition {
    override fun createLexer(project: Project?): Lexer = CpLexer()

    override fun createParser(project: Project?): PsiParser = CpPsiParser()

    override fun getFileNodeType(): IFileElementType = FILE

    override fun getCommentTokens(): TokenSet = CpTypes.COMMENT_TOKENS

    override fun getStringLiteralElements(): TokenSet = CpTypes.STRING_TOKENS

    override fun createElement(node: ASTNode): PsiElement = CpPsiElement(node)

    override fun createFile(viewProvider: FileViewProvider): PsiFile = CpFile(viewProvider)

    override fun getWhitespaceTokens(): TokenSet = TokenSet.create(TokenType.WHITE_SPACE)

    override fun spaceExistenceTypeBetweenTokens(left: ASTNode, right: ASTNode): ParserDefinition.SpaceRequirements =
        ParserDefinition.SpaceRequirements.MAY

    private companion object {
        private val FILE = IFileElementType(CpLanguage)
    }
}

class CpPsiParser : PsiParser {
    override fun parse(root: com.intellij.psi.tree.IElementType, builder: PsiBuilder): ASTNode {
        val marker = builder.mark()
        while (!builder.eof()) {
            builder.advanceLexer()
        }
        marker.done(root)
        return builder.treeBuilt
    }
}

class CpFile(viewProvider: FileViewProvider) : PsiFileBase(viewProvider, CpLanguage) {
    override fun getFileType() = CpFileType.INSTANCE

    override fun toString(): String = "cp file"
}

class CpPsiElement(node: ASTNode) : ASTWrapperPsiElement(node)
