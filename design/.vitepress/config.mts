import { defineConfig } from "vitepress";

const docsSidebar = (indexLink: string) => [
  {
    text: "开始学习",
    items: [
      { text: "学习路线", link: indexLink },
      { text: "语法导览", link: "/docs/syntax" },
      { text: "标准库导览", link: "/docs/stdlib" },
      { text: "示例导览", link: "/docs/examples" },
    ],
  },
  {
    text: "语言语法",
    collapsed: false,
    items: [
      { text: "类型系统", link: "/docs/type_system" },
      { text: "初始化", link: "/docs/initial" },
      { text: "模块", link: "/docs/module" },
      { text: "流程控制", link: "/docs/flow" },
      { text: "结构体与 impl", link: "/docs/struct" },
      { text: "Enum", link: "/docs/enum" },
      { text: "Variant 与 match", link: "/docs/variant" },
      { text: "运算符", link: "/docs/operator" },
      { text: "类型转换", link: "/docs/cast" },
      { text: "Lambda 与函数值", link: "/docs/lambda" },
      { text: "泛型", link: "/docs/generic" },
      { text: "元编程与反射基础", link: "/docs/meta" },
      { text: "Concept", link: "/docs/concept" },
      { text: "所有权、借用与移动", link: "/docs/ownership" },
    ],
  },
  {
    text: "标准库",
    collapsed: false,
    items: [
      { text: "标准库导览", link: "/docs/stdlib" },
      { text: "Core", link: "/docs/std_core" },
      { text: "迭代协议", link: "/docs/iteration" },
      { text: "Memory", link: "/docs/std_memory" },
      { text: "Text", link: "/docs/std_text" },
      { text: "Collections", link: "/docs/std_collections" },
      { text: "Compare", link: "/docs/std_compare" },
      { text: "Ranges", link: "/docs/std_ranges" },
      { text: "Algorithm", link: "/docs/std_algorithm" },
      { text: "IO", link: "/docs/std_io" },
      { text: "FS", link: "/docs/std_fs" },
    ],
  },
  {
    text: "底层与互操作",
    collapsed: true,
    items: [
      { text: "错误处理", link: "/docs/error_handling" },
      { text: "底层内存分配", link: "/docs/memory_allocation" },
      { text: "Opaque alias", link: "/docs/opaque_alias" },
      { text: 'extern "C"', link: "/docs/extern_c" },
    ],
  },
];

export default defineConfig({
  lang: "zh-CN",
  title: "KCP 语言文档",
  description: "KCP 语言的语法、语义与标准库学习文档",
  cleanUrls: true,
  lastUpdated: true,
  srcExclude: ["README.md", "docs/README.md"],
  markdown: {
    languageAlias: {
      cp: "cpp",
      llvm: "txt",
    },
    lineNumbers: true,
  },
  themeConfig: {
    logo: undefined,
    search: {
      provider: "local",
    },
    nav: [
      { text: "学习路线", link: "/" },
      { text: "语法", link: "/docs/syntax" },
      { text: "标准库", link: "/docs/stdlib" },
      { text: "示例", link: "/docs/examples" },
    ],
    sidebar: {
      "/": docsSidebar("/"),
      "/docs/": docsSidebar("/docs/"),
    },
    outline: {
      level: [2, 3],
      label: "本页目录",
    },
    docFooter: {
      prev: "上一页",
      next: "下一页",
    },
    lastUpdated: {
      text: "最后更新",
      formatOptions: {
        dateStyle: "medium",
        timeStyle: "short",
      },
    },
  },
});
