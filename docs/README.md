# OpticManager Documentation

This directory contains the auto-generated Doxygen documentation for the OpticManager project.

## 📖 Viewing Documentation

Open `html/index.html` in your web browser to view the complete documentation.

## 📚 What's Included

- **Class Documentation**: All classes with detailed method descriptions
- **File Documentation**: Every source file with Doxygen comments
- **Architecture Diagrams**: Mermaid diagrams from ARCHITECTURE.md
- **Code Examples**: Usage examples for all major components
- **Call Graphs**: Function call relationships
- **Dependency Graphs**: File and class dependencies

## 🔄 Regenerating Documentation

To regenerate the documentation after code changes:

```bash
doxygen Doxyfile
```

## 📋 Documentation Structure

```
docs/
├── html/              # HTML documentation (open index.html)
│   ├── index.html    # Main page
│   ├── annotated.html # Class list
│   ├── files.html    # File list
│   └── ...
└── README.md         # This file
```

## 🎯 Quick Links (after opening html/index.html)

- **Main Page**: Project overview from README.md
- **Classes**: Browse all classes and their members
- **Files**: Browse source files with syntax highlighting
- **Namespaces**: App, Drivers, Hal, Utils
- **Architecture**: System diagrams and design

## ⚙️ Configuration

Documentation is configured via `Doxyfile` in the project root.

Key settings:
- **Language**: C++20
- **Output**: HTML only
- **Source Browser**: Enabled
- **Call Graphs**: Disabled (requires Graphviz)
- **Search**: Enabled

---

Generated with Doxygen 1.13.2
