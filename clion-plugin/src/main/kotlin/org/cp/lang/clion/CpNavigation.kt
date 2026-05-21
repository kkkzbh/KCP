package org.cp.lang.clion

import com.intellij.codeInsight.navigation.actions.GotoDeclarationHandler
import com.intellij.navigation.DirectNavigationProvider
import com.intellij.openapi.actionSystem.DataContext
import com.intellij.openapi.diagnostic.Logger
import com.intellij.openapi.editor.Editor
import com.intellij.openapi.fileEditor.FileDocumentManager
import com.intellij.openapi.progress.ProcessCanceledException
import com.intellij.openapi.project.Project
import com.intellij.openapi.roots.ProjectRootManager
import com.intellij.openapi.vfs.LocalFileSystem
import com.intellij.openapi.vfs.VfsUtilCore
import com.intellij.openapi.vfs.VirtualFile
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiFile
import com.intellij.psi.PsiManager
import java.util.concurrent.CancellationException

class CpGotoDeclarationHandler : GotoDeclarationHandler {
    override fun getGotoDeclarationTargets(sourceElement: PsiElement?, offset: Int, editor: Editor?): Array<PsiElement>? {
        CpNavigationLog.debug {
            "goto handler source=${sourceElement.cpDebug()} offset=$offset editorFile=${editor?.document?.cpDebugFilePath()}"
        }
        if (isCtrlMousePreviewQuery()) {
            CpNavigationLog.debug { "goto handler skipped for ctrl mouse preview" }
            return null
        }
        val element = sourceElement?.cpNavigationElementNearOffset(offset) ?: return null
        if (element.containingFile?.language != CpLanguage) {
            CpNavigationLog.debug { "goto handler ignored non-cp source=${element.cpDebug()}" }
            return null
        }

        val target = cpResolveDeclarationForAction(element, editor) ?: return null
        CpNavigationLog.debug { "goto handler target=${target.cpDebug()}" }
        return arrayOf(target)
    }

    override fun getActionText(context: DataContext): String? = null
}

class CpDirectNavigationProvider : DirectNavigationProvider {
    override fun getNavigationElement(element: PsiElement): PsiElement? {
        CpNavigationLog.debug { "direct provider source=${element.cpDebug()}" }
        val source = element.cpNavigationElementNearOffset(element.textRange.endOffset) ?: return null
        if (source.containingFile?.language != CpLanguage) {
            CpNavigationLog.debug { "direct provider ignored non-cp source=${source.cpDebug()}" }
            return null
        }
        val target = cpResolveDeclarationForAction(source, null)
        CpNavigationLog.debug { "direct provider target=${target.cpDebug()}" }
        return target
    }
}

internal fun cpResolveDeclarationForAction(element: PsiElement, editor: Editor?): PsiElement? {
    return runCatching {
        CpSemanticDeclarationResolver.resolveForAction(element, editor)
    }.getOrElse { exception ->
        exception.rethrowIfControlFlow()
        null
    }
}

internal fun cpResolveDeclarationForReference(element: PsiElement): PsiElement? {
    return runCatching {
        if (isCtrlMousePreviewQuery()) {
            CpSemanticDeclarationResolver.resolve(element, null)
        } else {
            CpSemanticDeclarationResolver.resolveForAction(element, null)
        }
    }.getOrElse { exception ->
        exception.rethrowIfControlFlow()
        null
    }
}

object CpSemanticDeclarationResolver {
    fun resolve(element: PsiElement, editor: Editor?): PsiElement? {
        val file = element.containingFile ?: return null
        val cache = CpSemanticCache.get(file.project)
        cache.current(file)?.let { cached ->
            CpNavigationLog.debug { "semantic cache hit source=${element.cpDebug()} entries=${cached.result.navigation.size}" }
            return resolveFromInspection(file, element, cached.request, cached.result)
        }

        CpNavigationLog.debug { "semantic cache miss source=${element.cpDebug()} refresh=background" }
        cache.requestRefresh(file, file.activeText(editor))
        return null
    }

    fun resolveNow(element: PsiElement, editor: Editor?): PsiElement? {
        val file = element.containingFile ?: return null
        CpNavigationLog.debug { "semantic compute start source=${element.cpDebug()}" }
        val cached = CpSemanticCache.get(file.project).computeNow(file, file.activeText(editor)) ?: run {
            CpNavigationLog.debug { "semantic compute skipped source=${element.cpDebug()} fileLanguage=${file.language.id}" }
            return null
        }
        CpNavigationLog.debug { "semantic compute done source=${element.cpDebug()} entries=${cached.result.navigation.size}" }
        return resolveFromInspection(file, element, cached.request, cached.result)
    }

    fun resolveForAction(element: PsiElement, editor: Editor?): PsiElement? {
        val file = element.containingFile ?: return null
        val cache = CpSemanticCache.get(file.project)
        cache.current(file)?.let { cached ->
            CpNavigationLog.debug { "semantic action cache hit source=${element.cpDebug()} entries=${cached.result.navigation.size}" }
            val target = resolveFromInspection(file, element, cached.request, cached.result)
            if (target != null) {
                return target
            }
            CpNavigationLog.debug { "semantic action cache unresolved source=${element.cpDebug()} recompute=now" }
        }
        return resolveNow(element, editor)
    }

    private fun resolveFromInspection(
        file: PsiFile,
        element: PsiElement,
        request: CpInspectionRequest,
        result: CpInspectionResult,
    ): PsiElement? {
        if (result.navigation.isEmpty()) {
            CpNavigationLog.debug { "semantic target miss source=${element.cpDebug()} entries=0" }
            return null
        }

        val range = element.textRange
        val target = result.navigation
            .filter { navigation ->
                navigation.sourceStartOffset >= range.startOffset &&
                    navigation.sourceEndOffset <= range.endOffset
            }
            .minByOrNull { it.sourceEndOffset - it.sourceStartOffset }
            ?: run {
                CpNavigationLog.debug { "semantic target miss source=${element.cpDebug()} range=$range entries=${result.navigation.size}" }
                return null
            }

        val targetFile = targetFile(file, request, target.targetFile) ?: run {
            CpNavigationLog.debug { "semantic target file miss source=${element.cpDebug()} targetPath=${target.targetFile}" }
            return null
        }
        val targetElement = targetFile.findElementAt(target.targetStartOffset)?.cpNavigationTargetElement()
        CpNavigationLog.debug {
            "semantic target match source=${element.cpDebug()} targetPath=${target.targetFile} target=${targetElement.cpDebug()}"
        }
        return targetElement
    }

    private fun targetFile(activeFile: PsiFile, request: CpInspectionRequest, path: String): PsiFile? {
        val normalized = normalizePath(path)
        if (normalizePath(activeFile.virtualFile?.path ?: activeFile.name) == normalized) {
            return activeFile
        }
        if (request.files.none { normalizePath(it.path) == normalized }) {
            return null
        }

        val psiManager = PsiManager.getInstance(activeFile.project)
        return sequenceOf(normalized, path)
            .mapNotNull { candidate ->
                LocalFileSystem.getInstance().findFileByPath(candidate)
                    ?: LocalFileSystem.getInstance().refreshAndFindFileByPath(candidate)
            }
            .plus(sequenceOf(findProjectVirtualFile(activeFile.project, normalized)))
            .filterNotNull()
            .mapNotNull(psiManager::findFile)
            .firstOrNull()
    }
}

private fun PsiFile.activeText(editor: Editor?): String =
    editor?.document?.text
        ?: virtualFile?.let { FileDocumentManager.getInstance().getDocument(it)?.text }
        ?: text

internal object CpNavigationKinds {
    val referenceTypes = setOf(
        CpElements.NAME_EXPRESSION,
        CpElements.TYPE_NAME,
        CpElements.IMPORT_NAME,
        CpElements.MEMBER_NAME,
        CpElements.ASSOCIATED_NAME,
        CpElements.LOOP_LABEL,
        CpElements.VARIANT_CASE_NAME,
        CpElements.FIELD_DECLARATION,
    )

    val declarationTypes = setOf(
        CpElements.MODULE_NAME,
        CpElements.FUNCTION_NAME,
        CpElements.PARAMETER_NAME,
        CpElements.LOCAL_DECLARATION,
        CpElements.MATCH_BINDING,
        CpElements.TYPE_NAME,
        CpElements.GENERIC_PARAMETER_NAME,
        CpElements.FIELD_DECLARATION,
        CpElements.ENUM_CASE_NAME,
        CpElements.VARIANT_CASE_NAME,
        CpElements.LOOP_LABEL,
    )
}

internal fun PsiElement.cpNavigationElement(): PsiElement? {
    var current: PsiElement? = this
    while (current != null) {
        if (current.cpElementType() in CpNavigationKinds.referenceTypes) {
            return current
        }
        current = current.parent
    }
    return null
}

internal fun PsiElement.cpNavigationElementNearOffset(offset: Int): PsiElement? {
    cpNavigationElement()?.let { return it }

    val file = containingFile ?: return null
    return sequenceOf(offset, offset - 1)
        .filter { it >= 0 && it < file.textLength }
        .mapNotNull { file.findElementAt(it)?.cpNavigationElement() }
        .firstOrNull()
}

internal fun PsiElement.cpNavigationTargetElement(): PsiElement? {
    var current: PsiElement? = this
    while (current != null) {
        if (current.cpElementType() in CpNavigationKinds.declarationTypes) {
            return current
        }
        current = current.parent
    }
    return null
}

private fun normalizePath(path: String): String =
    cpNormalizePath(path)

private object CpNavigationLog {
    private val log = Logger.getInstance("org.cp.lang.clion.navigation")

    fun debug(message: () -> String) {
        if (log.isDebugEnabled) {
            log.debug(message())
        }
    }
}

private fun PsiElement?.cpDebug(): String {
    if (this == null) {
        return "<null>"
    }
    val file = containingFile
    val virtualFile = file?.virtualFile
    return buildString {
        append(text.take(80).replace('\n', ' '))
        append(":")
        append(cpElementType())
        append("@")
        append(textRange)
        append(" file=")
        append(virtualFile?.path ?: file?.name)
        append(" language=")
        append(file?.language?.id)
        append(" fileType=")
        append(virtualFile?.fileType?.name)
    }
}

private fun com.intellij.openapi.editor.Document.cpDebugFilePath(): String? =
    FileDocumentManager.getInstance().getFile(this)?.path

internal fun isCtrlMousePreviewQuery(): Boolean =
    Thread.currentThread().stackTrace.any { frame ->
        frame.className.startsWith("com.intellij.codeInsight.navigation.CtrlMouse") ||
            frame.methodName == "getCtrlMouseData"
    }

private fun findProjectVirtualFile(project: Project, path: String): VirtualFile? {
    for (root in ProjectRootManager.getInstance(project).contentRoots) {
        var result: VirtualFile? = null
        VfsUtilCore.iterateChildrenRecursively(root, null) { virtualFile ->
            if (normalizePath(virtualFile.path) == path) {
                result = virtualFile
                return@iterateChildrenRecursively false
            }
            true
        }
        if (result != null) {
            return result
        }
    }
    return null
}

private fun Throwable.rethrowIfControlFlow() {
    if (this is ProcessCanceledException || this is CancellationException) {
        throw this
    }
    cause?.rethrowIfControlFlow()
}
