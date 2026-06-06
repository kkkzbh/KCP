# design 文档站

`design/` 是 KCP 语言的设计资料和 VitePress 文档站根目录。公开网站页面以 `index.md` 和 `docs/index.md` 为入口；本
`README.md` 只作为仓库内维护说明，不参与 VitePress 构建。

## 维护边界

文档描述当前编译器和标准库已经接受、检查、lower 或运行的语言边界，不写成脱离实现的愿景稿。更新页面时优先核对这些来源：

- `parser/`、`semantic/`、`compiler/`、`runtime/` 和 `std/` 中的实现。
- `test/semantic/`、`test/compiler/`、`test/fixtures/` 中的语义、IR 和运行测试。
- `design/examples/` 与 `test/fixtures/examples/` 的镜像示例。

如果某个语法或库接口只是计划能力，正文应明确写成“不支持”“尚未接入”或“不要依赖”，不能混入可用能力列表。

## 目录关系

- `index.md`：VitePress 首页，对应网站 `/`。
- `docs/index.md`：学习路线入口，对应网站 `/docs/`。
- `docs/*.md`：语法、静态语义和标准库说明，对应网站 `/docs/<name>`。
- `examples/`：按主题组织的 `.cp` 示例项目，供文档引用和读者对照。
- `.vitepress/config.mts`：VitePress 站点配置，维护导航栏、侧边栏、本地搜索和 Markdown 渲染规则。
- `package.json`：文档站的本地预览、构建和 Cloudflare Pages 部署脚本。

仓库根目录的 `std/` 保存标准库源码。`docs/` 中的标准库说明应和这些普通 KCP 模块保持一致；涉及 runtime ABI 的页面还要核对
`runtime/` 中的 C++ 实现。

## 本地命令

在本目录执行：

```bash
npm install
npm run dev
npm run build
npm run preview
```

构建产物输出到 `.vitepress/dist/`，该目录不提交。

## 部署

Cloudflare Pages 项目名为 `kcp-docs`，生产入口域名为 `kcp.kkkzbh.cn`。

常规部署脚本：

```bash
npm run deploy:pages
```

如果本机 Wrangler 未登录，可以先构建，再通过 Cloudflare API 或已登录环境上传 `.vitepress/dist/`。
