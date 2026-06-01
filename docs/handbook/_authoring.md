# Handbook 写作约定

本站由 MkDocs Material 构建。协作者补充文档时请遵循：

## 标题层级

| Markdown | 用途 |
|----------|------|
| `#` | 页标题（每页一个） |
| `##` | 节 — 出现在右侧目录 |
| `###` | 小节 — 出现在右侧目录 |
| `####` 及以下 | 默认不进右侧目录 |

## 导航

- 在 `docs/handbook/runtime/` 下按与 `src/Runtime` 对应的路径新建 `.md`。
- 在同一目录的 `.pages` 中增加 `nav` 项（勿改根 `mkdocs.yml` 的「运行时」入口）。
- 草稿文件名建议 `_` 前缀，避免被 `...` 通配扫入（若目录使用通配）。

## 时效性

- 页脚 **最后更新** 日期来自 Git 最后一次修改该文件的提交，无需手写 front matter。
- 合并到 `main` 后由 GitHub Pages 部署；本地预览请使用 `mkdocs serve`。

本页不列入站点导航（无 `.pages` 引用即可；若被扫入可在 `docs/handbook/.pages` 中 `hide: true`）。
