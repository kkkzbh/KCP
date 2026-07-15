package org.cp.lang.clion

import java.net.JarURLConnection
import java.net.URL
import java.nio.file.Path

internal fun cpNormalizePath(path: String): String =
    runCatching { Path.of(path).toAbsolutePath().normalize().toString() }.getOrDefault(path)

internal fun cpSourceImportRoots(source: Path, projectBasePath: String?, stdlibRoot: Path?): List<Path> =
    sequenceOf(
        source.parent,
        projectBasePath?.takeUnless { it.isBlank() }?.let { Path.of(it) },
        stdlibRoot,
    )
        .filterNotNull()
        .map { it.toAbsolutePath().normalize() }
        .distinct()
        .toList()

internal fun Path?.parentsFromSelf(): Sequence<Path> =
    generateSequence(this?.toAbsolutePath()?.normalize()) { it.parent }

internal fun cpPluginNativePath(name: String): Path? =
    cpPluginRoot()?.resolve("native/linux-x86_64")?.resolve(name)

private fun cpPluginRoot(): Path? {
    val pluginClass = CpPlugin::class.java
    val resourceName = "/${pluginClass.name.replace('.', '/')}.class"
    val classResource = pluginClass.getResource(resourceName) ?: return null
    return cpPluginRootFromClassResource(classResource)
}

internal fun cpPluginRootFromClassResource(classResource: URL): Path? {
    val connection = runCatching { classResource.openConnection() }.getOrNull() as? JarURLConnection ?: return null
    val pluginJar = runCatching { Path.of(connection.jarFileURL.toURI()) }.getOrNull() ?: return null
    val libraryDirectory = pluginJar.parent?.takeIf { it.fileName.toString() == "lib" } ?: return null
    return libraryDirectory.parent?.toAbsolutePath()?.normalize()
}
