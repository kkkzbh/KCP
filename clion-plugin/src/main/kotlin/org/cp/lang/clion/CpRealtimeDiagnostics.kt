package org.cp.lang.clion

import com.intellij.codeInsight.daemon.DaemonCodeAnalyzer
import com.intellij.execution.RunManager
import com.intellij.execution.RunManagerListener
import com.intellij.execution.RunnerAndConfigurationSettings
import com.intellij.openapi.application.ApplicationManager
import com.intellij.openapi.editor.EditorFactory
import com.intellij.openapi.editor.event.DocumentEvent
import com.intellij.openapi.editor.event.DocumentListener
import com.intellij.openapi.fileEditor.FileDocumentManager
import com.intellij.openapi.fileEditor.FileEditorManager
import com.intellij.openapi.project.Project
import com.intellij.openapi.startup.ProjectActivity
import com.intellij.psi.PsiFile
import com.intellij.psi.PsiManager
import com.intellij.util.Alarm

internal class CpRealtimeDiagnosticsActivity : ProjectActivity {
    override suspend fun execute(project: Project) {
        CpRealtimeDiagnosticsRefresher(project).install()
    }
}

private class CpRealtimeDiagnosticsRefresher(
    private val project: Project,
) {
    private val alarm = Alarm(Alarm.ThreadToUse.SWING_THREAD, project)

    fun install() {
        EditorFactory.getInstance().eventMulticaster.addDocumentListener(
            object : DocumentListener {
                override fun documentChanged(event: DocumentEvent) {
                    val file = FileDocumentManager.getInstance().getFile(event.document) ?: return
                    if (file.extension != CpFileType.INSTANCE.defaultExtension) {
                        return
                    }
                    scheduleRestart()
                }
            },
            project,
        )

        project.messageBus.connect(project).subscribe(
            RunManagerListener.TOPIC,
            object : RunManagerListener {
                override fun runConfigurationSelected(settings: RunnerAndConfigurationSettings?) {
                    scheduleRestart()
                }

                override fun runConfigurationChanged(settings: RunnerAndConfigurationSettings) {
                    if (settings === RunManager.getInstance(project).selectedConfiguration) {
                        scheduleRestart()
                    }
                }
            },
        )
    }

    private fun scheduleRestart() {
        if (project.isDisposed) {
            return
        }
        alarm.cancelAllRequests()
        alarm.addRequest(::restartOpenCpFiles, restartDelayMillis)
    }

    private fun restartOpenCpFiles() {
        if (project.isDisposed) {
            return
        }

        val files = ApplicationManager.getApplication().runReadAction<List<PsiFile>> {
            FileEditorManager.getInstance(project).openFiles
                .asSequence()
                .filter { it.isValid && !it.isDirectory && it.extension == CpFileType.INSTANCE.defaultExtension }
                .mapNotNull { PsiManager.getInstance(project).findFile(it) }
                .filter { it.language == CpLanguage }
                .toList()
        }

        val analyzer = DaemonCodeAnalyzer.getInstance(project)
        for (file in files) {
            analyzer.restart(file)
        }
    }

    private companion object {
        const val restartDelayMillis = 120
    }
}
