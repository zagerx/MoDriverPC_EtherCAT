# mo_ecat_core

EtherCAT 主站核心库，零 Qt、无外部可执行文件依赖。

## 目录

- `include/mo_ecat/`：公共头文件
- `src/`：核心库实现
- `third_party/SOEM`：SOEM 子模块
- `examples/cli/`：过渡期命令行示例
- `docs/`：设计文档与笔记

## 快速构建

```bash
# 核心库
cmake -B build .
cmake --build build -j$(nproc)

# CLI 示例
cd examples/cli
cmake -B build -DCMAKE_PREFIX_PATH=../../build
cmake --build build -j$(nproc)
```

详见 [docs/others/使用文档.md](docs/others/使用文档.md)。
