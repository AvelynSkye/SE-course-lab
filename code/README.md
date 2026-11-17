# 校园活动票务与报名系统

## 构建

```powershell
cmake -S . -B build
cmake --build build
```

MSVC 构建会自动启用 C++17、UTF-8 和 `/W4`。

## 测试

```powershell
ctest --test-dir build --output-on-failure
```

## 风格检查

```powershell
cmake --build build --target format-check
```

自动格式化：

```powershell
cmake --build build --target format
```
