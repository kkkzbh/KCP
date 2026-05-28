package org.cp.lang.clion

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
