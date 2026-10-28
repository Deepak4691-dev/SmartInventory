#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#define MAX_PRODUCTS 1000
#define NAME_LEN 80
#define CATEGORY_LEN 50
#define DATA_FILE "data/products.dat"
#define REPORT_FILE "reports/inventory_report.txt"

typedef struct {
    int id;
    char name[NAME_LEN];
    char category[CATEGORY_LEN];
    int quantity;
    double price;
    int minStock;
    int year, month, day;
} Product;

Product products[MAX_PRODUCTS];
int productCount = 0;

/* ---------- Folder and input utilities ---------- */

void createFolder(const char *path) {
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0777);
#endif
}

void readLine(const char *prompt, char *buffer, size_t size) {
    int ch;
    printf("%s", prompt);

    if (fgets(buffer, (int)size, stdin) == NULL) {
        buffer[0] = '\0';
        return;
    }

    size_t len = strlen(buffer);

    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    } else {
        while ((ch = getchar()) != '\n' && ch != EOF) {}
    }
}

int getInt(const char *prompt, int min, int max) {
    char line[100], *end;
    long value;

    while (1) {
        readLine(prompt, line, sizeof(line));
        errno = 0;
        value = strtol(line, &end, 10);

        while (isspace((unsigned char)*end)) end++;

        if (line[0] != '\0' && *end == '\0' &&
            errno != ERANGE && value >= min && value <= max)
            return (int)value;

        printf("Invalid input. Enter a number from %d to %d.\n",
               min, max);
    }
}

double getDouble(const char *prompt, double min) {
    char line[100], *end;
    double value;

    while (1) {
        readLine(prompt, line, sizeof(line));
        errno = 0;
        value = strtod(line, &end);

        while (isspace((unsigned char)*end)) end++;

        if (line[0] != '\0' && *end == '\0' &&
            errno != ERANGE && value >= min && value < 1e12)
            return value;

        printf("Invalid input. Enter a non-negative amount.\n");
    }
}

void readText(const char *prompt, char *out, size_t size) {
    while (1) {
        readLine(prompt, out, size);

        if (strlen(out) > 0) {
            return;
        }

        printf("This field cannot be empty.\n");
    }
}

/* ---------- Date utilities ---------- */

int leapYear(int year) {
    return year % 400 == 0 ||
           (year % 4 == 0 && year % 100 != 0);
}

int validDate(int y, int m, int d) {
    int days[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};

    if (y < 1900 || y > 9999 || m < 1 || m > 12)
        return 0;

    if (leapYear(y))
        days[2] = 29;

    return d >= 1 && d <= days[m];
}

void readDate(int *y, int *m, int *d) {
    char line[100];
    int yy, mm, dd;
    char extra;

    while (1) {
        readLine("Expiry date (YYYY-MM-DD): ", line, sizeof(line));

        if (sscanf(line, "%d-%d-%d %c", &yy, &mm, &dd, &extra) == 3
            && validDate(yy, mm, dd)) {
            *y = yy;
            *m = mm;
            *d = dd;
            return;
        }

        printf("Invalid date. Example: 2027-05-21\n");
    }
}

time_t dateTimestamp(int y, int m, int d) {
    struct tm date = {0};
    date.tm_year = y - 1900;
    date.tm_mon = m - 1;
    date.tm_mday = d;
    date.tm_hour = 12;
    date.tm_isdst = -1;
    return mktime(&date);
}

int daysUntilExpiry(const Product *p) {
    time_t now = time(NULL);
    struct tm today = *localtime(&now);

    time_t todayDate = dateTimestamp(
        today.tm_year + 1900, today.tm_mon + 1, today.tm_mday);
    time_t expiryDate = dateTimestamp(p->year, p->month, p->day);

    return (int)difftime(expiryDate, todayDate) / 86400;
}

/* ---------- File handling ---------- */

void loadProducts(void) {
    FILE *fp = fopen(DATA_FILE, "rb");

    if (fp == NULL)
        return;

    productCount = 0;

    while (productCount < MAX_PRODUCTS &&
           fread(&products[productCount], sizeof(Product), 1, fp) == 1) {
        productCount++;
    }

    fclose(fp);
}

int saveProducts(void) {
    FILE *fp = fopen(DATA_FILE, "wb");

    if (fp == NULL) {
        printf("Error: Cannot save inventory data.\n");
        return 0;
    }

    size_t written = fwrite(products, sizeof(Product),
                            (size_t)productCount, fp);

    fclose(fp);

    if (written != (size_t)productCount) {
        printf("Error while writing inventory data.\n");
        return 0;
    }

    return 1;
}

/* ---------- Search and display ---------- */

int findProduct(int id) {
    for (int i = 0; i < productCount; i++) {
        if (products[i].id == id)
            return i;
    }
    return -1;
}

void printHeader(void) {
    printf("\n%-6s %-20s %-14s %7s %10s %8s %-12s\n",
           "ID", "Name", "Category", "Qty", "Price",
           "MinStock", "Expiry");
    printf("-------------------------------------------------------------------------------\n");
}

void printProduct(const Product *p) {
    printf("%-6d %-20.20s %-14.14s %7d %10.2f %8d "
           "%04d-%02d-%02d\n",
           p->id, p->name, p->category, p->quantity,
           p->price, p->minStock, p->year, p->month, p->day);
}

void listProducts(void) {
    if (productCount == 0) {
        printf("\nNo products found.\n");
        return;
    }

    printHeader();

    for (int i = 0; i < productCount; i++)
        printProduct(&products[i]);

    printf("\nTotal products: %d\n", productCount);
}

/* ---------- Product operations ---------- */

void addProduct(void) {
    if (productCount >= MAX_PRODUCTS) {
        printf("Inventory is full.\n");
        return;
    }

    Product p = {0};

    p.id = getInt("Product ID: ", 1, 2147483647);

    if (findProduct(p.id) != -1) {
        printf("That product ID already exists.\n");
        return;
    }

    readText("Product name: ", p.name, sizeof(p.name));
    readText("Category: ", p.category, sizeof(p.category));
    p.quantity = getInt("Quantity: ", 0, 1000000000);
    p.price = getDouble("Price per unit: ", 0.0);
    p.minStock = getInt("Minimum stock alert level: ",
                        0, 1000000000);

    readDate(&p.year, &p.month, &p.day);

    products[productCount++] = p;

    if (saveProducts())
        printf("Product added successfully.\n");
}

void searchProduct(void) {
    int id = getInt("Enter product ID: ", 1, 2147483647);
    int index = findProduct(id);

    if (index == -1) {
        printf("Product not found.\n");
        return;
    }

    printHeader();
    printProduct(&products[index]);

    int days = daysUntilExpiry(&products[index]);

    if (days < 0)
        printf("Status: EXPIRED (%d days ago)\n", -days);
    else if (days <= 30)
        printf("Status: EXPIRING SOON (%d days remaining)\n", days);
    else
        printf("Status: SAFE (%d days remaining)\n", days);
}

void updateStock(void) {
    int id = getInt("Enter product ID: ", 1, 2147483647);
    int index = findProduct(id);

    if (index == -1) {
        printf("Product not found.\n");
        return;
    }

    printf("Current quantity: %d\n", products[index].quantity);
    products[index].quantity =
        getInt("Enter new quantity: ", 0, 1000000000);

    if (saveProducts())
        printf("Stock updated successfully.\n");
}

void deleteProduct(void) {
    int id = getInt("Enter product ID to delete: ",
                    1, 2147483647);
    int index = findProduct(id);

    if (index == -1) {
        printf("Product not found.\n");
        return;
    }

    printf("Deleting: %s\n", products[index].name);

    for (int i = index; i < productCount - 1; i++)
        products[i] = products[i + 1];

    productCount--;

    if (saveProducts())
        printf("Product deleted successfully.\n");
}

void expiryReport(void) {
    int found = 0;

    printf("\n--- EXPIRY REPORT ---\n");
    printHeader();

    for (int i = 0; i < productCount; i++) {
        int days = daysUntilExpiry(&products[i]);

        if (days <= 30) {
            printProduct(&products[i]);

            if (days < 0)
                printf("       STATUS: EXPIRED (%d days ago)\n", -days);
            else
                printf("       STATUS: EXPIRING (%d days left)\n", days);

            found++;
        }
    }

    if (!found)
        printf("No expired or soon-to-expire products.\n");
}

void lowStockReport(void) {
    int found = 0;

    printf("\n--- LOW STOCK REPORT ---\n");
    printHeader();

    for (int i = 0; i < productCount; i++) {
        if (products[i].quantity <= products[i].minStock) {
            printProduct(&products[i]);
            found++;
        }
    }

    if (!found)
        printf("No low-stock products.\n");
}

void inventoryValue(void) {
    double total = 0.0;

    for (int i = 0; i < productCount; i++)
        total += products[i].quantity * products[i].price;

    printf("\nTotal inventory value: %.2f\n", total);
}

/* ---------- Sorting ---------- */

void sortProducts(void) {
    if (productCount < 2) {
        printf("Not enough products to sort.\n");
        return;
    }

    printf("\n1. Sort by expiry date (earliest first)\n");
    printf("2. Sort by quantity (lowest first)\n");

    int choice = getInt("Choose sorting option: ", 1, 2);

    for (int i = 0; i < productCount - 1; i++) {
        for (int j = 0; j < productCount - i - 1; j++) {
            int swap = 0;

            if (choice == 1) {
                time_t a = dateTimestamp(products[j].year,
                                         products[j].month,
                                         products[j].day);
                time_t b = dateTimestamp(products[j+1].year,
                                         products[j+1].month,
                                         products[j+1].day);
                swap = a > b;
            } else {
                swap = products[j].quantity >
                       products[j+1].quantity;
            }

            if (swap) {
                Product temp = products[j];
                products[j] = products[j+1];
                products[j+1] = temp;
            }
        }
    }

    saveProducts();
    printf("\nSorted inventory:\n");
    listProducts();
}

/* ---------- Report generation ---------- */

void generateReport(void) {
    FILE *fp = fopen(REPORT_FILE, "w");

    if (fp == NULL) {
        printf("Cannot create report file.\n");
        return;
    }

    double totalValue = 0.0;
    int low = 0, expired = 0, soon = 0;

    time_t now = time(NULL);
    struct tm *today = localtime(&now);

    fprintf(fp, "SMART INVENTORY MANAGEMENT REPORT\n");
    fprintf(fp, "Generated: %04d-%02d-%02d\n",
            today->tm_year + 1900, today->tm_mon + 1,
            today->tm_mday);
    fprintf(fp, "========================================\n\n");

    fprintf(fp, "%-6s %-20s %-14s %7s %10s %-12s\n",
            "ID", "Name", "Category", "Qty", "Price", "Expiry");

    for (int i = 0; i < productCount; i++) {
        Product *p = &products[i];

        fprintf(fp, "%-6d %-20.20s %-14.14s %7d %10.2f "
                    "%04d-%02d-%02d\n",
                p->id, p->name, p->category, p->quantity,
                p->price, p->year, p->month, p->day);

        totalValue += p->quantity * p->price;

        if (p->quantity <= p->minStock)
            low++;

        int days = daysUntilExpiry(p);

        if (days < 0)
            expired++;
        else if (days <= 30)
            soon++;
    }

    fprintf(fp, "\nTotal product records: %d\n", productCount);
    fprintf(fp, "Total inventory value: %.2f\n", totalValue);
    fprintf(fp, "Low-stock products: %d\n", low);
    fprintf(fp, "Expired products: %d\n", expired);
    fprintf(fp, "Products expiring within 30 days: %d\n", soon);

    fclose(fp);

    printf("Report saved to %s\n", REPORT_FILE);
}

/* ---------- Main menu ---------- */

int main(void) {
    createFolder("data");
    createFolder("reports");
    loadProducts();

    int choice;

    do {
        printf("\n========================================\n");
        printf("   SMART INVENTORY MANAGEMENT SYSTEM\n");
        printf("========================================\n");
        printf("1.  Add Product\n");
        printf("2.  View All Products\n");
        printf("3.  Search Product\n");
        printf("4.  Update Stock\n");
        printf("5.  Delete Product\n");
        printf("6.  Check Expiry Dates\n");
        printf("7.  Low Stock Report\n");
        printf("8.  Sort Products\n");
        printf("9.  Calculate Inventory Value\n");
        printf("10. Generate Inventory Report\n");
        printf("0.  Exit\n");
        printf("----------------------------------------\n");

        choice = getInt("Enter your choice: ", 0, 10);

        switch (choice) {
            case 1: addProduct(); break;
            case 2: listProducts(); break;
            case 3: searchProduct(); break;
            case 4: updateStock(); break;
            case 5: deleteProduct(); break;
            case 6: expiryReport(); break;
            case 7: lowStockReport(); break;
            case 8: sortProducts(); break;
            case 9: inventoryValue(); break;
            case 10: generateReport(); break;
            case 0: printf("Thank you for using the system!\n"); break;
        }

    } while (choice != 0);

    return 0;
}
