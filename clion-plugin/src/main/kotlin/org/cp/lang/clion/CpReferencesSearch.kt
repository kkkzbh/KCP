package org.cp.lang.clion

import com.intellij.openapi.application.ApplicationManager
import com.intellij.openapi.fileEditor.FileDocumentManager
import com.intellij.openapi.progress.ProgressManager
import com.intellij.openapi.vfs.VfsUtilCore
import com.intellij.openapi.vfs.VirtualFile
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiFile
import com.intellij.psi.PsiManager
import com.intellij.psi.PsiReference
import com.intellij.psi.search.FileTypeIndex
import com.intellij.psi.search.GlobalSearchScope
import com.intellij.psi.search.searches.ReferencesSearch
import com.intellij.util.Processor
import com.intellij.util.QueryExecutor
import java.nio.file.Path

class CpReferencesSearch : QueryExecutor<PsiReference, ReferencesSearch.SearchParameters> {
    override fun execute(queryParameters: ReferencesSearch.SearchParameters, consumer: Processor<in PsiReference>): Boolean {
        val target = queryParameters.elementToSearch
        val targetInfo = readAction {
            val declaration = target.cpNavigationTargetElement() ?: return@readAction null
            if (declaration.containingFile?.language != CpLanguage ||
                declaration.cpElementType() !in CpNavigationKinds.declarationTypes
            ) {
                return@readAction null
            }

            SearchTarget(
                path = declaration.containingFile?.virtualFile?.path?.let(::normalizeSearchPath) ?: return@readAction null,
                range = declaration.textRange.startOffset until declaration.textRange.endOffset,
                text = declaration.text,
                project = declaration.project,
                virtualFile = declaration.containingFile?.virtualFile,
            )
        } ?: return true

        val project = targetInfo.project
        val psiManager = PsiManager.getInstance(project)
        val searchScope = queryParameters.effectiveSearchScope
        val emitted = mutableSetOf<Pair<String, IntRange>>()
        val projectFiles = readAction {
            val files = linkedMapOf<String, VirtualFile>()
            targetInfo.virtualFile
                ?.takeIf { it.isValid && !it.isDirectory && searchScope.contains(it) }
                ?.let { files[normalizeSearchPath(it.path)] = it }
            for (file in FileTypeIndex.getFiles(CpFileType.INSTANCE, GlobalSearchScope.projectScope(project))) {
                if (file.isValid && !file.isDirectory && searchScope.contains(file)) {
                    files[normalizeSearchPath(file.path)] = file
                }
            }
            files.values.toList()
        }

        for (virtualFile in projectFiles) {
            ProgressManager.checkCanceled()
            val candidateContext = readAction {
                if (!virtualFile.isValid || virtualFile.isDirectory || !searchScope.contains(virtualFile)) {
                    return@readAction null
                }

                val activeText = loadSearchText(virtualFile)
                if (!activeText.contains(targetInfo.text)) {
                    return@readAction null
                }

                val sourceFile = psiManager.findFile(virtualFile) ?: return@readAction null
                CpProjectSnapshotCollector.prepare(sourceFile, activeText)?.let { context ->
                    SearchCandidateContext(virtualFile, sourceFile, context)
                }
            } ?: continue
            val request = candidateContext.context.resolve()
            if (request.files.none { normalizeSearchPath(it.path) == targetInfo.path }) {
                continue
            }
            val candidate = SearchCandidate(
                virtualFile = virtualFile,
                sourceFile = candidateContext.sourceFile,
                request = request,
            )

            val result = candidate.cachedOrInspect(project) ?: continue
            for (navigation in result.navigation) {
                if (normalizeSearchPath(navigation.targetFile) != targetInfo.path ||
                    navigation.targetStartOffset != targetInfo.range.first ||
                    navigation.targetEndOffset != targetInfo.range.last + 1
                ) {
                    continue
                }

                val reference = readAction {
                    psiManager.findFile(candidate.virtualFile)
                        ?.findElementAt(navigation.sourceStartOffset)
                        ?.cpNavigationElement()
                        ?.reference
                }
                    ?: continue
                val key = candidate.virtualFile.path to (navigation.sourceStartOffset until navigation.sourceEndOffset)
                if (emitted.add(key) && !consumer.process(reference)) {
                    return false
                }
            }
        }

        return true
    }

    private fun loadSearchText(virtualFile: VirtualFile): String =
        FileDocumentManager.getInstance().getDocument(virtualFile)?.text
            ?: VfsUtilCore.loadText(virtualFile)

    private fun <T> readAction(block: () -> T): T =
        ApplicationManager.getApplication().runReadAction<T>(block)
}

private fun SearchCandidate.cachedOrInspect(project: com.intellij.openapi.project.Project): CpInspectionResult? {
    val cache = CpSemanticCache.get(project)
    cache.current(sourceFile)?.takeIf { it.request.sameInputAs(request) }?.let {
        return it.result
    }

    val result = CpHelperRunner.inspectOrNull(request) ?: return null
    cache.store(request, result, cpModificationCount(project))
    return result
}

private fun CpInspectionRequest.sameInputAs(other: CpInspectionRequest): Boolean =
    activeFile == other.activeFile &&
        files.size == other.files.size &&
        files.zip(other.files).all { (left, right) -> left.path == right.path && left.text == right.text }

private fun normalizeSearchPath(path: String): String =
    runCatching { Path.of(path).toAbsolutePath().normalize().toString() }.getOrDefault(path)

private data class SearchTarget(
    val path: String,
    val range: IntRange,
    val text: String,
    val project: com.intellij.openapi.project.Project,
    val virtualFile: VirtualFile?,
)

private data class SearchCandidate(
    val virtualFile: VirtualFile,
    val sourceFile: PsiFile,
    val request: CpInspectionRequest,
)

private data class SearchCandidateContext(
    val virtualFile: VirtualFile,
    val sourceFile: PsiFile,
    val context: CpSnapshotContext,
)
