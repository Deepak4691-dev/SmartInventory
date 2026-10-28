# Smart Inventory Management System

A comprehensive, high-performance Inventory Management System developed in C with both a **Modern Native Windows GUI** and an interactive **Command-Line Interface (CLI)**.

Both applications share the exact same binary data storage (`data/products.dat`) and reporting file (`reports/inventory_report.txt`), allowing seamless interoperability.

---

## 🚀 Key Features

### 1. Modern Desktop GUI (`SmartInventoryGUI.exe`)
* **KPI Dashboard Cards**: Real-time summary metric cards at the top displaying:
  * **Total Products** count
  * **Total Inventory Value** ($)
  * **Low Stock Alerts** (highlighted in red when items need replenishment)
  * **Expiring / Expired** counts (highlighted with urgent badges)
* **Real-time Live Search**: Filter products instantaneously as you type across:
  * **Product ID**
  * **Product Name**
  * **Category**
* **Quick Filter View**:
  * *All Products*
  * *Low Stock Only* (`Quantity <= Min Stock`)
  * *Expiring Soon* (within 30 days)
  * *Expired Products Only*
  * *Safe Stock Only*
* **Interactive Data Table (Report ListView)**:
  * Columns: `ID`, `Product Name`, `Category`, `Qty`, `Price`, `Total ($)`, `Min Stock`, `Expiry Date`, `Days Left`, `Status`.
  * **Column Sorting**: Click on any column header to sort ascending or descending.
  * **Visual Color Coding**: Soft-red highlights for expired items, amber for items expiring soon, and yellow/red for low-stock items.
* **Product Management**:
  * **Add Product**: Modal form with input validation, duplicate ID protection, and date validation.
  * **Edit Product**: Modify name, category, price, min stock threshold, and expiry date.
  * **Quick Adjust Stock**: Instant `[-10]`, `[-1]`, `[+1]`, `[+10]` adjustments or direct entry.
  * **Delete Product**: Safe deletion with confirmation prompt.
  * **Right-Click Context Menu**: Quick access to edit, adjust stock, delete, or generate reports on any row.
* **Reporting**:
  * Built-in **Inventory Report Generator & Viewer**: Generate and view the full text report inside a viewer window or open directly in Notepad.

### 2. Command-Line Interface (`main.exe`)
* Original terminal-based menu for headless or keyboard-only environments:
  * Add Product, View All, Search by ID, Update Stock, Delete, Expiry Report, Low Stock Report, Multi-criteria Sort, Calculate Value, and Generate Report.

---

## 📁 Project Structure

```
SmartInventory/
├── build_gui.bat                 # One-click batch build script for the GUI
├── gui_main.c                    # Complete native Windows GUI implementation (C / Win32)
├── main.c                        # Original CLI implementation
├── SmartInventoryGUI.exe         # Compiled standalone Windows GUI executable
├── main.exe                      # Compiled CLI executable
├── data/
│   └── products.dat              # Shared binary inventory database (168-byte struct format)
├── reports/
│   └── inventory_report.txt      # Formatted inventory status reports
└── .vscode/
    └── tasks.json                # VS Code build tasks configuration
```

---

## 🛠️ How to Build & Run

### Running the GUI Application
Double-click `SmartInventoryGUI.exe` or execute from PowerShell / Command Prompt:
```cmd
.\SmartInventoryGUI.exe
```

### Rebuilding the GUI
Run the included build script:
```cmd
.\build_gui.bat
```
*(Requires Visual Studio 2022 / MSVC build tools).*

### Rebuilding the CLI
```cmd
cl.exe /O2 /D_CRT_SECURE_NO_WARNINGS main.c /link /out:main.exe
```
Or in VS Code, press `Ctrl + Shift + B` and choose `Build CLI (main.exe)`.

---

## 💾 Data Format Specifications

Each product is stored in `data/products.dat` using the exact 168-byte C struct:
```c
typedef struct {
    int id;               // 4 bytes (offset 0)
    char name[80];        // 80 bytes (offset 4)
    char category[50];    // 50 bytes (offset 84)
    // 2 bytes padding    // offset 134..135
    int quantity;         // 4 bytes (offset 136)
    // 4 bytes padding    // offset 140..143
    double price;         // 8 bytes (offset 144)
    int minStock;         // 4 bytes (offset 152)
    int year, month, day; // 12 bytes (offsets 156, 160, 164)
} Product;
```
