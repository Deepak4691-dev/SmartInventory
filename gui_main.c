#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <direct.h>
#include <shellapi.h>

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

#define MAX_PRODUCTS 1000
#define NAME_LEN 80
#define CATEGORY_LEN 50
#define DATA_FILE "data/products.dat"
#define REPORT_FILE "reports/inventory_report.txt"

/* Product structure identical to main.c */
typedef struct {
    int id;
    char name[NAME_LEN];
    char category[CATEGORY_LEN];
    int quantity;
    double price;
    int minStock;
    int year, month, day;
} Product;

static Product products[MAX_PRODUCTS];
static int productCount = 0;

/* Filtered indices currently displayed in ListView */
static int displayIndices[MAX_PRODUCTS];
static int displayCount = 0;

/* Control IDs */
enum {
    ID_SEARCH_EDIT = 1001,
    ID_FILTER_COMBO,
    ID_BTN_CLEAR_SEARCH,
    ID_BTN_ADD,
    ID_BTN_EDIT,
    ID_BTN_STOCK,
    ID_BTN_DELETE,
    ID_BTN_REPORT,
    ID_BTN_REFRESH,
    ID_LISTVIEW,
    ID_STATUSBAR,

    /* Context menu */
    IDM_EDIT = 2001,
    IDM_STOCK,
    IDM_DELETE,
    IDM_REPORT,

    /* Product Dialog controls */
    ID_DLG_ID = 3001,
    ID_DLG_NAME,
    ID_DLG_CATEGORY,
    ID_DLG_QTY,
    ID_DLG_PRICE,
    ID_DLG_MINSTOCK,
    ID_DLG_DATE,
    ID_DLG_BTN_OK,
    ID_DLG_BTN_CANCEL,

    /* Quick Stock Dialog controls */
    ID_STK_LBL_INFO = 4001,
    ID_STK_EDIT_QTY,
    ID_STK_BTN_M10,
    ID_STK_BTN_M1,
    ID_STK_BTN_P1,
    ID_STK_BTN_P10,
    ID_STK_BTN_OK,
    ID_STK_BTN_CANCEL,

    /* Report Viewer controls */
    ID_RPT_EDIT = 5001,
    ID_RPT_BTN_NOTEPAD,
    ID_RPT_BTN_CLOSE
};

/* Global Handles */
static HINSTANCE g_hInstance;
static HWND g_hMainWnd;
static HWND g_hListView;
static HWND g_hStatusBar;
static HWND g_hSearchEdit;
static HWND g_hFilterCombo;
static HWND g_hBtnClearSearch;
static HWND g_hBtnAdd;
static HWND g_hBtnEdit;
static HWND g_hBtnStock;
static HWND g_hBtnDelete;
static HWND g_hBtnReport;
static HWND g_hBtnRefresh;

/* Fonts */
static HFONT g_hFontNormal;
static HFONT g_hFontBold;
static HFONT g_hFontLargeBold;
static HFONT g_hFontTitle;
static HFONT g_hFontFixed;

/* Sorting state */
static int g_sortColumn = 0;
static int g_sortAscending = 1;

/* Summary counts */
static int g_summaryTotalProducts = 0;
static double g_summaryTotalValue = 0.0;
static int g_summaryLowStock = 0;
static int g_summaryExpiringSoon = 0;
static int g_summaryExpired = 0;

/* Product dialog transfer state */
static Product g_dlgProduct;
static int g_dlgIsEdit = 0;
static int g_dlgSuccess = 0;

/* Stock dialog transfer state */
static int g_stockEditIndex = -1;
static int g_stockNewQty = 0;

/* ---------- Utility Functions ---------- */

void createFolder(const char *path) {
    _mkdir(path);
}

int leapYear(int year) {
    return year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
}

int validDate(int y, int m, int d) {
    int days[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if (y < 1900 || y > 9999 || m < 1 || m > 12) return 0;
    if (leapYear(y)) days[2] = 29;
    return d >= 1 && d <= days[m];
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
    time_t todayDate = dateTimestamp(today.tm_year + 1900, today.tm_mon + 1, today.tm_mday);
    time_t expiryDate = dateTimestamp(p->year, p->month, p->day);
    return (int)(difftime(expiryDate, todayDate) / 86400.0);
}

int findProduct(int id) {
    for (int i = 0; i < productCount; i++) {
        if (products[i].id == id) return i;
    }
    return -1;
}

void loadProducts(void) {
    FILE *fp = fopen(DATA_FILE, "rb");
    if (fp == NULL) {
        productCount = 0;
        return;
    }
    productCount = 0;
    while (productCount < MAX_PRODUCTS &&
           fread(&products[productCount], sizeof(Product), 1, fp) == 1) {
        productCount++;
    }
    fclose(fp);
}

int saveProducts(void) {
    createFolder("data");
    FILE *fp = fopen(DATA_FILE, "wb");
    if (fp == NULL) return 0;
    size_t written = fwrite(products, sizeof(Product), (size_t)productCount, fp);
    fclose(fp);
    return (written == (size_t)productCount);
}

void generateReportFile(void) {
    createFolder("reports");
    FILE *fp = fopen(REPORT_FILE, "w");
    if (fp == NULL) return;

    double totalValue = 0.0;
    int low = 0, expired = 0, soon = 0;
    time_t now = time(NULL);
    struct tm *today = localtime(&now);

    fprintf(fp, "SMART INVENTORY MANAGEMENT REPORT\n");
    fprintf(fp, "Generated: %04d-%02d-%02d\n",
            today->tm_year + 1900, today->tm_mon + 1, today->tm_mday);
    fprintf(fp, "========================================\n\n");
    fprintf(fp, "%-6s %-20s %-14s %7s %10s %-12s\n",
            "ID", "Name", "Category", "Qty", "Price", "Expiry");

    for (int i = 0; i < productCount; i++) {
        Product *p = &products[i];
        fprintf(fp, "%-6d %-20.20s %-14.14s %7d %10.2f %04d-%02d-%02d\n",
                p->id, p->name, p->category, p->quantity,
                p->price, p->year, p->month, p->day);

        totalValue += p->quantity * p->price;
        if (p->quantity <= p->minStock) low++;
        int days = daysUntilExpiry(p);
        if (days < 0) expired++;
        else if (days <= 30) soon++;
    }

    fprintf(fp, "\nTotal product records: %d\n", productCount);
    fprintf(fp, "Total inventory value: %.2f\n", totalValue);
    fprintf(fp, "Low-stock products: %d\n", low);
    fprintf(fp, "Expired products: %d\n", expired);
    fprintf(fp, "Products expiring within 30 days: %d\n", soon);
    fclose(fp);
}

/* Helper to convert string to lowercase for case-insensitive search */
static void toLowerStr(const char *src, char *dst, size_t maxLen) {
    size_t i = 0;
    for (; src[i] != '\0' && i + 1 < maxLen; i++) {
        dst[i] = (char)tolower((unsigned char)src[i]);
    }
    dst[i] = '\0';
}

/* Refresh summary calculations */
void updateSummaryMetrics(void) {
    g_summaryTotalProducts = productCount;
    g_summaryTotalValue = 0.0;
    g_summaryLowStock = 0;
    g_summaryExpiringSoon = 0;
    g_summaryExpired = 0;

    for (int i = 0; i < productCount; i++) {
        g_summaryTotalValue += products[i].quantity * products[i].price;
        if (products[i].quantity <= products[i].minStock) {
            g_summaryLowStock++;
        }
        int days = daysUntilExpiry(&products[i]);
        if (days < 0) {
            g_summaryExpired++;
        } else if (days <= 30) {
            g_summaryExpiringSoon++;
        }
    }
}

/* Sort comparator */
int compareProductsByColumn(int idxA, int idxB, int col, int asc) {
    const Product *a = &products[idxA];
    const Product *b = &products[idxB];
    int result = 0;

    switch (col) {
        case 0: /* ID */
            result = (a->id > b->id) - (a->id < b->id);
            break;
        case 1: /* Name */
            result = _stricmp(a->name, b->name);
            break;
        case 2: /* Category */
            result = _stricmp(a->category, b->category);
            break;
        case 3: /* Quantity */
            result = (a->quantity > b->quantity) - (a->quantity < b->quantity);
            break;
        case 4: /* Price */
            result = (a->price > b->price) - (a->price < b->price);
            break;
        case 5: { /* Total Value */
            double valA = a->quantity * a->price;
            double valB = b->quantity * b->price;
            result = (valA > valB) - (valA < valB);
            break;
        }
        case 6: /* Min Stock */
            result = (a->minStock > b->minStock) - (a->minStock < b->minStock);
            break;
        case 7: { /* Expiry Date */
            time_t tA = dateTimestamp(a->year, a->month, a->day);
            time_t tB = dateTimestamp(b->year, b->month, b->day);
            result = (tA > tB) - (tA < tB);
            break;
        }
        case 8: { /* Days Left */
            int daysA = daysUntilExpiry(a);
            int daysB = daysUntilExpiry(b);
            result = (daysA > daysB) - (daysA < daysB);
            break;
        }
        default:
            result = (a->id > b->id) - (a->id < b->id);
            break;
    }
    return asc ? result : -result;
}

/* Populate ListView based on search query and filter selection */
void refreshListView(void) {
    char query[128] = {0};
    GetWindowTextA(g_hSearchEdit, query, sizeof(query));
    char queryLower[128];
    toLowerStr(query, queryLower, sizeof(queryLower));

    int filterSel = (int)SendMessage(g_hFilterCombo, CB_GETCURSEL, 0, 0);
    if (filterSel == CB_ERR) filterSel = 0;

    displayCount = 0;

    for (int i = 0; i < productCount; i++) {
        Product *p = &products[i];
        int days = daysUntilExpiry(p);
        int isLowStock = (p->quantity <= p->minStock);
        int isExpired = (days < 0);
        int isExpiringSoon = (days >= 0 && days <= 30);

        /* Filter check */
        if (filterSel == 1 && !isLowStock) continue;
        if (filterSel == 2 && !isExpiringSoon) continue;
        if (filterSel == 3 && !isExpired) continue;
        if (filterSel == 4 && (isExpired || isExpiringSoon || isLowStock)) continue;

        /* Search query check across ID, Name, Category */
        if (queryLower[0] != '\0') {
            char idStr[32];
            char nameLower[NAME_LEN + 1];
            char catLower[CATEGORY_LEN + 1];
            sprintf(idStr, "%d", p->id);
            toLowerStr(p->name, nameLower, sizeof(nameLower));
            toLowerStr(p->category, catLower, sizeof(catLower));

            if (strstr(idStr, queryLower) == NULL &&
                strstr(nameLower, queryLower) == NULL &&
                strstr(catLower, queryLower) == NULL) {
                continue;
            }
        }

        displayIndices[displayCount++] = i;
    }

    /* Sort display items */
    for (int i = 0; i < displayCount - 1; i++) {
        for (int j = 0; j < displayCount - i - 1; j++) {
            if (compareProductsByColumn(displayIndices[j], displayIndices[j+1], g_sortColumn, g_sortAscending) > 0) {
                int temp = displayIndices[j];
                displayIndices[j] = displayIndices[j+1];
                displayIndices[j+1] = temp;
            }
        }
    }

    /* Update ListView */
    SendMessage(g_hListView, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(g_hListView);

    for (int i = 0; i < displayCount; i++) {
        int realIdx = displayIndices[i];
        Product *p = &products[realIdx];
        int days = daysUntilExpiry(p);

        char buf[64];
        LVITEMA lvi = {0};
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        sprintf(buf, "%d", p->id);
        lvi.pszText = buf;
        lvi.lParam = (LPARAM)realIdx;
        ListView_InsertItem(g_hListView, &lvi);

        ListView_SetItemText(g_hListView, i, 1, p->name);
        ListView_SetItemText(g_hListView, i, 2, p->category);

        sprintf(buf, "%d", p->quantity);
        ListView_SetItemText(g_hListView, i, 3, buf);

        sprintf(buf, "Rs. %.2f", p->price);
        ListView_SetItemText(g_hListView, i, 4, buf);

        sprintf(buf, "Rs. %.2f", p->quantity * p->price);
        ListView_SetItemText(g_hListView, i, 5, buf);

        sprintf(buf, "%d", p->minStock);
        ListView_SetItemText(g_hListView, i, 6, buf);

        sprintf(buf, "%04d-%02d-%02d", p->year, p->month, p->day);
        ListView_SetItemText(g_hListView, i, 7, buf);

        if (days < 0) {
            sprintf(buf, "%d d ago", -days);
        } else {
            sprintf(buf, "%d d", days);
        }
        ListView_SetItemText(g_hListView, i, 8, buf);

        /* Status text */
        if (days < 0) {
            strcpy(buf, "EXPIRED");
        } else if (days <= 30) {
            strcpy(buf, "EXPIRING SOON");
        } else if (p->quantity <= p->minStock) {
            strcpy(buf, "LOW STOCK");
        } else {
            strcpy(buf, "SAFE");
        }
        ListView_SetItemText(g_hListView, i, 9, buf);
    }
    SendMessage(g_hListView, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_hListView, NULL, TRUE);

    /* Update Status Bar */
    char statusText[256];
    sprintf(statusText, "  Showing %d of %d products  |  Total Stock Value: Rs. %.2f  |  Low Stock: %d  |  Expired: %d  |  Expiring in 30d: %d",
            displayCount, productCount, g_summaryTotalValue, g_summaryLowStock, g_summaryExpired, g_summaryExpiringSoon);
    SendMessageA(g_hStatusBar, SB_SETTEXTA, 0, (LPARAM)statusText);
}

/* Get currently selected product real index in products[], or -1 */
int getSelectedProductRealIndex(void) {
    int selIndex = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
    if (selIndex == -1 || selIndex >= displayCount) return -1;
    LVITEMA lvi = {0};
    lvi.iItem = selIndex;
    lvi.mask = LVIF_PARAM;
    if (ListView_GetItem(g_hListView, &lvi)) {
        return (int)lvi.lParam;
    }
    return -1;
}

/* ---------- Custom UI Painting ---------- */

void drawSummaryCards(HDC hdc, RECT rect) {
    HBRUSH hBgBrush = CreateSolidBrush(RGB(243, 244, 246));
    FillRect(hdc, &rect, hBgBrush);
    DeleteObject(hBgBrush);

    int cardMargin = 15;
    int cardCount = 4;
    int totalWidth = rect.right - rect.left - (cardMargin * (cardCount + 1));
    int cardWidth = totalWidth / cardCount;
    int cardHeight = 85;
    int cardY = 12;

    struct {
        const char *title;
        char valueStr[64];
        COLORREF accentColor;
        COLORREF textColor;
    } cards[4];

    cards[0].title = "TOTAL PRODUCTS";
    sprintf(cards[0].valueStr, "%d Items", g_summaryTotalProducts);
    cards[0].accentColor = RGB(59, 130, 246); /* Blue */
    cards[0].textColor = RGB(30, 58, 138);

    cards[1].title = "TOTAL INVENTORY VALUE";
    sprintf(cards[1].valueStr, "Rs. %.2f", g_summaryTotalValue);
    cards[1].accentColor = RGB(16, 185, 129); /* Green */
    cards[1].textColor = RGB(6, 95, 70);

    cards[2].title = "LOW STOCK ALERTS";
    sprintf(cards[2].valueStr, "%d %s", g_summaryLowStock, g_summaryLowStock == 1 ? "Item" : "Items");
    cards[2].accentColor = (g_summaryLowStock > 0) ? RGB(239, 68, 68) : RGB(107, 114, 128);
    cards[2].textColor = (g_summaryLowStock > 0) ? RGB(153, 27, 27) : RGB(55, 65, 81);

    cards[3].title = "EXPIRING / EXPIRED";
    sprintf(cards[3].valueStr, "%d / %d", g_summaryExpiringSoon, g_summaryExpired);
    cards[3].accentColor = (g_summaryExpired > 0) ? RGB(220, 38, 38) : ((g_summaryExpiringSoon > 0) ? RGB(245, 158, 11) : RGB(107, 114, 128));
    cards[3].textColor = (g_summaryExpired > 0) ? RGB(153, 27, 27) : ((g_summaryExpiringSoon > 0) ? RGB(146, 64, 14) : RGB(55, 65, 81));

    SetBkMode(hdc, TRANSPARENT);

    for (int i = 0; i < cardCount; i++) {
        int x = cardMargin + i * (cardWidth + cardMargin);
        RECT cardRect = { x, cardY, x + cardWidth, cardY + cardHeight };

        /* Card background */
        HBRUSH hCardBrush = CreateSolidBrush(RGB(255, 255, 255));
        HPEN hCardPen = CreatePen(PS_SOLID, 1, RGB(229, 231, 235));
        HGDIOBJ oldBrush = SelectObject(hdc, hCardBrush);
        HGDIOBJ oldPen = SelectObject(hdc, hCardPen);
        RoundRect(hdc, cardRect.left, cardRect.top, cardRect.right, cardRect.bottom, 10, 10);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(hCardBrush);
        DeleteObject(hCardPen);

        /* Left accent bar */
        RECT barRect = { x, cardY + 5, x + 5, cardY + cardHeight - 5 };
        HBRUSH hBarBrush = CreateSolidBrush(cards[i].accentColor);
        FillRect(hdc, &barRect, hBarBrush);
        DeleteObject(hBarBrush);

        /* Title text */
        SelectObject(hdc, g_hFontBold);
        SetTextColor(hdc, RGB(107, 114, 128));
        RECT titleRect = { x + 16, cardY + 12, x + cardWidth - 10, cardY + 30 };
        DrawTextA(hdc, cards[i].title, -1, &titleRect, DT_SINGLELINE | DT_LEFT | DT_VCENTER);

        /* Value text */
        SelectObject(hdc, g_hFontLargeBold);
        SetTextColor(hdc, cards[i].textColor);
        RECT valRect = { x + 16, cardY + 32, x + cardWidth - 10, cardY + 75 };
        DrawTextA(hdc, cards[i].valueStr, -1, &valRect, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    }
}

/* ---------- Dialogs Implementation ---------- */

/* Add / Edit Product Dialog Procedure */
static LRESULT CALLBACK ProductDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HWND hCtl;
            int xLbl = 25, xEdit = 135, wEdit = 230, hRow = 32, y = 20;

            /* Set Window Title */
            SetWindowTextA(hWnd, g_dlgIsEdit ? "Edit Product" : "Add New Product");

            /* Product ID */
            hCtl = CreateWindowA("STATIC", "Product ID:", WS_CHILD | WS_VISIBLE | SS_RIGHT, xLbl, y + 4, 100, 20, hWnd, NULL, g_hInstance, NULL);
            SendMessage(hCtl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            hCtl = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_NUMBER | (g_dlgIsEdit ? ES_READONLY : 0),
                                  xEdit, y, wEdit, 24, hWnd, (HMENU)ID_DLG_ID, g_hInstance, NULL);
            SendMessage(hCtl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            if (g_dlgIsEdit) {
                char buf[32]; sprintf(buf, "%d", g_dlgProduct.id);
                SetWindowTextA(hCtl, buf);
            }

            /* Product Name */
            y += hRow;
            hCtl = CreateWindowA("STATIC", "Name:", WS_CHILD | WS_VISIBLE | SS_RIGHT, xLbl, y + 4, 100, 20, hWnd, NULL, g_hInstance, NULL);
            SendMessage(hCtl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            hCtl = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_dlgIsEdit ? g_dlgProduct.name : "", WS_CHILD | WS_VISIBLE,
                                  xEdit, y, wEdit, 24, hWnd, (HMENU)ID_DLG_NAME, g_hInstance, NULL);
            SendMessage(hCtl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            /* Category */
            y += hRow;
            hCtl = CreateWindowA("STATIC", "Category:", WS_CHILD | WS_VISIBLE | SS_RIGHT, xLbl, y + 4, 100, 20, hWnd, NULL, g_hInstance, NULL);
            SendMessage(hCtl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            hCtl = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_dlgIsEdit ? g_dlgProduct.category : "", WS_CHILD | WS_VISIBLE,
                                  xEdit, y, wEdit, 24, hWnd, (HMENU)ID_DLG_CATEGORY, g_hInstance, NULL);
            SendMessage(hCtl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            /* Quantity */
            y += hRow;
            hCtl = CreateWindowA("STATIC", "Quantity:", WS_CHILD | WS_VISIBLE | SS_RIGHT, xLbl, y + 4, 100, 20, hWnd, NULL, g_hInstance, NULL);
            SendMessage(hCtl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            hCtl = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_NUMBER,
                                  xEdit, y, wEdit, 24, hWnd, (HMENU)ID_DLG_QTY, g_hInstance, NULL);
            SendMessage(hCtl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            if (g_dlgIsEdit) {
                char buf[32]; sprintf(buf, "%d", g_dlgProduct.quantity);
                SetWindowTextA(hCtl, buf);
            }

            /* Price */
            y += hRow;
            hCtl = CreateWindowA("STATIC", "Unit Price (Rs.):", WS_CHILD | WS_VISIBLE | SS_RIGHT, xLbl, y + 4, 100, 20, hWnd, NULL, g_hInstance, NULL);
            SendMessage(hCtl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            hCtl = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE,
                                  xEdit, y, wEdit, 24, hWnd, (HMENU)ID_DLG_PRICE, g_hInstance, NULL);
            SendMessage(hCtl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            if (g_dlgIsEdit) {
                char buf[32]; sprintf(buf, "%.2f", g_dlgProduct.price);
                SetWindowTextA(hCtl, buf);
            }

            /* Min Stock */
            y += hRow;
            hCtl = CreateWindowA("STATIC", "Min Stock Alert:", WS_CHILD | WS_VISIBLE | SS_RIGHT, xLbl, y + 4, 100, 20, hWnd, NULL, g_hInstance, NULL);
            SendMessage(hCtl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            hCtl = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_NUMBER,
                                  xEdit, y, wEdit, 24, hWnd, (HMENU)ID_DLG_MINSTOCK, g_hInstance, NULL);
            SendMessage(hCtl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            if (g_dlgIsEdit) {
                char buf[32]; sprintf(buf, "%d", g_dlgProduct.minStock);
                SetWindowTextA(hCtl, buf);
            } else {
                SetWindowTextA(hCtl, "5");
            }

            /* Expiry Date */
            y += hRow;
            hCtl = CreateWindowA("STATIC", "Expiry (YYYY-MM-DD):", WS_CHILD | WS_VISIBLE | SS_RIGHT, xLbl - 20, y + 4, 120, 20, hWnd, NULL, g_hInstance, NULL);
            SendMessage(hCtl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            hCtl = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE,
                                  xEdit, y, wEdit, 24, hWnd, (HMENU)ID_DLG_DATE, g_hInstance, NULL);
            SendMessage(hCtl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            if (g_dlgIsEdit) {
                char buf[32]; sprintf(buf, "%04d-%02d-%02d", g_dlgProduct.year, g_dlgProduct.month, g_dlgProduct.day);
                SetWindowTextA(hCtl, buf);
            } else {
                time_t now = time(NULL);
                struct tm *t = localtime(&now);
                char buf[32]; sprintf(buf, "%04d-%02d-%02d", t->tm_year + 1901, t->tm_mon + 1, t->tm_mday);
                SetWindowTextA(hCtl, buf);
            }

            /* Buttons */
            y += 42;
            hCtl = CreateWindowA("BUTTON", g_dlgIsEdit ? "Save Changes" : "Add Product", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                xEdit, y, 120, 32, hWnd, (HMENU)ID_DLG_BTN_OK, g_hInstance, NULL);
            SendMessage(hCtl, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            hCtl = CreateWindowA("BUTTON", "Cancel", WS_CHILD | WS_VISIBLE,
                                xEdit + 130, y, 90, 32, hWnd, (HMENU)ID_DLG_BTN_CANCEL, g_hInstance, NULL);
            SendMessage(hCtl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == ID_DLG_BTN_CANCEL) {
                DestroyWindow(hWnd);
                return 0;
            }
            if (id == ID_DLG_BTN_OK) {
                char bufId[32], bufName[NAME_LEN + 1], bufCat[CATEGORY_LEN + 1];
                char bufQty[32], bufPrice[32], bufMin[32], bufDate[64];

                GetWindowTextA(GetDlgItem(hWnd, ID_DLG_ID), bufId, sizeof(bufId));
                GetWindowTextA(GetDlgItem(hWnd, ID_DLG_NAME), bufName, sizeof(bufName));
                GetWindowTextA(GetDlgItem(hWnd, ID_DLG_CATEGORY), bufCat, sizeof(bufCat));
                GetWindowTextA(GetDlgItem(hWnd, ID_DLG_QTY), bufQty, sizeof(bufQty));
                GetWindowTextA(GetDlgItem(hWnd, ID_DLG_PRICE), bufPrice, sizeof(bufPrice));
                GetWindowTextA(GetDlgItem(hWnd, ID_DLG_MINSTOCK), bufMin, sizeof(bufMin));
                GetWindowTextA(GetDlgItem(hWnd, ID_DLG_DATE), bufDate, sizeof(bufDate));

                int prodId = atoi(bufId);
                if (prodId <= 0) {
                    MessageBoxA(hWnd, "Please enter a valid positive Product ID.", "Input Error", MB_ICONWARNING);
                    SetFocus(GetDlgItem(hWnd, ID_DLG_ID));
                    return 0;
                }

                if (!g_dlgIsEdit && findProduct(prodId) != -1) {
                    MessageBoxA(hWnd, "A product with this ID already exists. Please choose a unique ID.", "Duplicate ID", MB_ICONWARNING);
                    SetFocus(GetDlgItem(hWnd, ID_DLG_ID));
                    return 0;
                }

                if (strlen(bufName) == 0) {
                    MessageBoxA(hWnd, "Product name cannot be empty.", "Input Error", MB_ICONWARNING);
                    SetFocus(GetDlgItem(hWnd, ID_DLG_NAME));
                    return 0;
                }

                if (strlen(bufCat) == 0) {
                    MessageBoxA(hWnd, "Product category cannot be empty.", "Input Error", MB_ICONWARNING);
                    SetFocus(GetDlgItem(hWnd, ID_DLG_CATEGORY));
                    return 0;
                }

                int qty = atoi(bufQty);
                if (qty < 0) {
                    MessageBoxA(hWnd, "Quantity cannot be negative.", "Input Error", MB_ICONWARNING);
                    SetFocus(GetDlgItem(hWnd, ID_DLG_QTY));
                    return 0;
                }

                double price = atof(bufPrice);
                if (price < 0.0) {
                    MessageBoxA(hWnd, "Price cannot be negative.", "Input Error", MB_ICONWARNING);
                    SetFocus(GetDlgItem(hWnd, ID_DLG_PRICE));
                    return 0;
                }

                int minStock = atoi(bufMin);
                if (minStock < 0) {
                    MessageBoxA(hWnd, "Min stock alert cannot be negative.", "Input Error", MB_ICONWARNING);
                    SetFocus(GetDlgItem(hWnd, ID_DLG_MINSTOCK));
                    return 0;
                }

                int yy, mm, dd;
                char extra;
                if (sscanf(bufDate, "%d-%d-%d %c", &yy, &mm, &dd, &extra) != 3 || !validDate(yy, mm, dd)) {
                    MessageBoxA(hWnd, "Invalid expiry date format. Please use YYYY-MM-DD (e.g., 2027-05-21).", "Invalid Date", MB_ICONWARNING);
                    SetFocus(GetDlgItem(hWnd, ID_DLG_DATE));
                    return 0;
                }

                /* Populate g_dlgProduct */
                g_dlgProduct.id = prodId;
                strncpy(g_dlgProduct.name, bufName, sizeof(g_dlgProduct.name) - 1);
                g_dlgProduct.name[sizeof(g_dlgProduct.name) - 1] = '\0';
                strncpy(g_dlgProduct.category, bufCat, sizeof(g_dlgProduct.category) - 1);
                g_dlgProduct.category[sizeof(g_dlgProduct.category) - 1] = '\0';
                g_dlgProduct.quantity = qty;
                g_dlgProduct.price = price;
                g_dlgProduct.minStock = minStock;
                g_dlgProduct.year = yy;
                g_dlgProduct.month = mm;
                g_dlgProduct.day = dd;

                g_dlgSuccess = 1;
                DestroyWindow(hWnd);
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

/* Modal helper for Add/Edit product */
int showProductDialog(HWND hParent, int isEdit, Product *pProd) {
    g_dlgIsEdit = isEdit;
    g_dlgSuccess = 0;
    if (isEdit && pProd) {
        g_dlgProduct = *pProd;
    } else {
        memset(&g_dlgProduct, 0, sizeof(Product));
    }

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ProductDlgProc;
    wc.hInstance = g_hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "SmartInv_ProductDlg";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExA(&wc);

    int w = 420, h = 340;
    RECT rcParent;
    GetWindowRect(hParent, &rcParent);
    int x = rcParent.left + (rcParent.right - rcParent.left - w) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - h) / 2;

    HWND hDlg = CreateWindowExA(WS_EX_DLGMODALFRAME, "SmartInv_ProductDlg", "",
                               WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                               x, y, w, h, hParent, NULL, g_hInstance, NULL);

    EnableWindow(hParent, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);

    if (g_dlgSuccess && pProd) {
        *pProd = g_dlgProduct;
    }
    return g_dlgSuccess;
}

/* Quick Stock Dialog Procedure */
static LRESULT CALLBACK StockDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            Product *p = &products[g_stockEditIndex];
            char info[256];
            sprintf(info, "Product:  %s  (ID: %d)\nCurrent Quantity: %d", p->name, p->id, p->quantity);

            HWND hInfo = CreateWindowA("STATIC", info, WS_CHILD | WS_VISIBLE, 25, 20, 310, 40, hWnd, (HMENU)ID_STK_LBL_INFO, g_hInstance, NULL);
            SendMessage(hInfo, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            HWND hLbl = CreateWindowA("STATIC", "New Quantity:", WS_CHILD | WS_VISIBLE, 25, 75, 110, 20, hWnd, NULL, g_hInstance, NULL);
            SendMessage(hLbl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            char qBuf[32]; sprintf(qBuf, "%d", p->quantity);
            HWND hEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", qBuf, WS_CHILD | WS_VISIBLE | ES_NUMBER,
                                        140, 72, 100, 26, hWnd, (HMENU)ID_STK_EDIT_QTY, g_hInstance, NULL);
            SendMessage(hEdit, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            /* Quick modifier buttons */
            HWND b1 = CreateWindowA("BUTTON", "-10", WS_CHILD | WS_VISIBLE, 25, 115, 60, 28, hWnd, (HMENU)ID_STK_BTN_M10, g_hInstance, NULL);
            HWND b2 = CreateWindowA("BUTTON", "-1", WS_CHILD | WS_VISIBLE, 95, 115, 60, 28, hWnd, (HMENU)ID_STK_BTN_M1, g_hInstance, NULL);
            HWND b3 = CreateWindowA("BUTTON", "+1", WS_CHILD | WS_VISIBLE, 165, 115, 60, 28, hWnd, (HMENU)ID_STK_BTN_P1, g_hInstance, NULL);
            HWND b4 = CreateWindowA("BUTTON", "+10", WS_CHILD | WS_VISIBLE, 235, 115, 60, 28, hWnd, (HMENU)ID_STK_BTN_P10, g_hInstance, NULL);
            SendMessage(b1, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            SendMessage(b2, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            SendMessage(b3, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            SendMessage(b4, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            /* OK / Cancel */
            HWND bOk = CreateWindowA("BUTTON", "Update Stock", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                    55, 165, 110, 32, hWnd, (HMENU)ID_STK_BTN_OK, g_hInstance, NULL);
            HWND bCancel = CreateWindowA("BUTTON", "Cancel", WS_CHILD | WS_VISIBLE,
                                        175, 165, 90, 32, hWnd, (HMENU)ID_STK_BTN_CANCEL, g_hInstance, NULL);
            SendMessage(bOk, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);
            SendMessage(bCancel, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            HWND hEdit = GetDlgItem(hWnd, ID_STK_EDIT_QTY);
            char qBuf[32];
            GetWindowTextA(hEdit, qBuf, sizeof(qBuf));
            int curVal = atoi(qBuf);

            if (id == ID_STK_BTN_M10) { curVal = max(0, curVal - 10); sprintf(qBuf, "%d", curVal); SetWindowTextA(hEdit, qBuf); return 0; }
            if (id == ID_STK_BTN_M1)  { curVal = max(0, curVal - 1);  sprintf(qBuf, "%d", curVal); SetWindowTextA(hEdit, qBuf); return 0; }
            if (id == ID_STK_BTN_P1)  { curVal = curVal + 1; sprintf(qBuf, "%d", curVal); SetWindowTextA(hEdit, qBuf); return 0; }
            if (id == ID_STK_BTN_P10) { curVal = curVal + 10; sprintf(qBuf, "%d", curVal); SetWindowTextA(hEdit, qBuf); return 0; }

            if (id == ID_STK_BTN_CANCEL) {
                DestroyWindow(hWnd);
                return 0;
            }

            if (id == ID_STK_BTN_OK) {
                if (curVal < 0) {
                    MessageBoxA(hWnd, "Quantity cannot be negative.", "Input Error", MB_ICONWARNING);
                    return 0;
                }
                g_stockNewQty = curVal;
                g_dlgSuccess = 1;
                DestroyWindow(hWnd);
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

/* Modal helper for quick stock update */
int showStockDialog(HWND hParent, int prodIndex) {
    g_stockEditIndex = prodIndex;
    g_dlgSuccess = 0;

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = StockDlgProc;
    wc.hInstance = g_hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "SmartInv_StockDlg";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExA(&wc);

    int w = 340, h = 250;
    RECT rcParent;
    GetWindowRect(hParent, &rcParent);
    int x = rcParent.left + (rcParent.right - rcParent.left - w) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - h) / 2;

    HWND hDlg = CreateWindowExA(WS_EX_DLGMODALFRAME, "SmartInv_StockDlg", "Quick Update Stock",
                               WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                               x, y, w, h, hParent, NULL, g_hInstance, NULL);

    EnableWindow(hParent, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);

    if (g_dlgSuccess) {
        products[prodIndex].quantity = g_stockNewQty;
        saveProducts();
        generateReportFile();
        updateSummaryMetrics();
        refreshListView();
        InvalidateRect(g_hMainWnd, NULL, TRUE);
    }
    return g_dlgSuccess;
}

/* Report Viewer Dialog Procedure */
static LRESULT CALLBACK ReportDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HWND hEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                                        15, 15, 650, 420, hWnd, (HMENU)ID_RPT_EDIT, g_hInstance, NULL);
            SendMessage(hEdit, WM_SETFONT, (WPARAM)g_hFontFixed, TRUE);

            /* Read report file contents */
            FILE *fp = fopen(REPORT_FILE, "r");
            if (fp) {
                fseek(fp, 0, SEEK_END);
                long sz = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                char *buf = (char *)malloc(sz + 1);
                if (buf) {
                    size_t readBytes = fread(buf, 1, sz, fp);
                    buf[readBytes] = '\0';
                    SetWindowTextA(hEdit, buf);
                    free(buf);
                }
                fclose(fp);
            }

            HWND bNotepad = CreateWindowA("BUTTON", "Open in Notepad", WS_CHILD | WS_VISIBLE,
                                         200, 445, 140, 32, hWnd, (HMENU)ID_RPT_BTN_NOTEPAD, g_hInstance, NULL);
            HWND bClose = CreateWindowA("BUTTON", "Close", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                       350, 445, 100, 32, hWnd, (HMENU)ID_RPT_BTN_CLOSE, g_hInstance, NULL);
            SendMessage(bNotepad, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            SendMessage(bClose, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);
            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == ID_RPT_BTN_CLOSE) {
                DestroyWindow(hWnd);
                return 0;
            }
            if (id == ID_RPT_BTN_NOTEPAD) {
                ShellExecuteA(hWnd, "open", "notepad.exe", REPORT_FILE, NULL, SW_SHOWNORMAL);
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

void showReportViewer(HWND hParent) {
    generateReportFile();

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ReportDlgProc;
    wc.hInstance = g_hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "SmartInv_ReportDlg";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExA(&wc);

    int w = 695, h = 530;
    RECT rcParent;
    GetWindowRect(hParent, &rcParent);
    int x = rcParent.left + (rcParent.right - rcParent.left - w) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - h) / 2;

    HWND hDlg = CreateWindowExA(WS_EX_DLGMODALFRAME, "SmartInv_ReportDlg", "Generated Inventory Report",
                               WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                               x, y, w, h, hParent, NULL, g_hInstance, NULL);

    EnableWindow(hParent, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);
}

/* ---------- Main Window Procedure ---------- */

static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            /* Create Common Controls */
            INITCOMMONCONTROLSEX icex;
            icex.dwSize = sizeof(icex);
            icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
            InitCommonControlsEx(&icex);

            /* Fonts */
            g_hFontNormal = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            g_hFontBold = CreateFontA(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            g_hFontLargeBold = CreateFontA(-22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            g_hFontTitle = CreateFontA(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            g_hFontFixed = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");

            /* Status Bar */
            g_hStatusBar = CreateWindowExA(0, STATUSCLASSNAMEA, "", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                          0, 0, 0, 0, hWnd, (HMENU)ID_STATUSBAR, g_hInstance, NULL);
            SendMessage(g_hStatusBar, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            /* Search Label & Edit Box */
            HWND hLblSearch = CreateWindowA("STATIC", "Search:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                           15, 114, 52, 20, hWnd, NULL, g_hInstance, NULL);
            SendMessage(hLblSearch, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            g_hSearchEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE,
                                           70, 110, 240, 26, hWnd, (HMENU)ID_SEARCH_EDIT, g_hInstance, NULL);
            SendMessage(g_hSearchEdit, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            g_hBtnClearSearch = CreateWindowA("BUTTON", "Clear", WS_CHILD | WS_VISIBLE,
                                             315, 110, 60, 26, hWnd, (HMENU)ID_BTN_CLEAR_SEARCH, g_hInstance, NULL);
            SendMessage(g_hBtnClearSearch, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            /* Filter Combo */
            HWND hLblFilter = CreateWindowA("STATIC", "Filter:", WS_CHILD | WS_VISIBLE | SS_RIGHT,
                                           390, 114, 45, 20, hWnd, NULL, g_hInstance, NULL);
            SendMessage(hLblFilter, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            g_hFilterCombo = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                          440, 110, 210, 160, hWnd, (HMENU)ID_FILTER_COMBO, g_hInstance, NULL);
            SendMessage(g_hFilterCombo, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            SendMessageA(g_hFilterCombo, CB_ADDSTRING, 0, (LPARAM)"All Products");
            SendMessageA(g_hFilterCombo, CB_ADDSTRING, 0, (LPARAM)"Low Stock Only (Qty <= Min)");
            SendMessageA(g_hFilterCombo, CB_ADDSTRING, 0, (LPARAM)"Expiring Soon (<= 30 Days)");
            SendMessageA(g_hFilterCombo, CB_ADDSTRING, 0, (LPARAM)"Expired Products Only");
            SendMessageA(g_hFilterCombo, CB_ADDSTRING, 0, (LPARAM)"Safe Stock Only");
            SendMessage(g_hFilterCombo, CB_SETCURSEL, 0, 0);

            /* Action Buttons Toolbar */
            int btnY = 148;
            g_hBtnAdd = CreateWindowA("BUTTON", "+ Add Product", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     15, btnY, 115, 32, hWnd, (HMENU)ID_BTN_ADD, g_hInstance, NULL);
            SendMessage(g_hBtnAdd, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            g_hBtnEdit = CreateWindowA("BUTTON", "Edit Product", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                      135, btnY, 105, 32, hWnd, (HMENU)ID_BTN_EDIT, g_hInstance, NULL);
            SendMessage(g_hBtnEdit, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            g_hBtnStock = CreateWindowA("BUTTON", "Update Stock", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                       245, btnY, 110, 32, hWnd, (HMENU)ID_BTN_STOCK, g_hInstance, NULL);
            SendMessage(g_hBtnStock, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            g_hBtnDelete = CreateWindowA("BUTTON", "Delete", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        360, btnY, 85, 32, hWnd, (HMENU)ID_BTN_DELETE, g_hInstance, NULL);
            SendMessage(g_hBtnDelete, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            g_hBtnReport = CreateWindowA("BUTTON", "Generate Report", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        450, btnY, 130, 32, hWnd, (HMENU)ID_BTN_REPORT, g_hInstance, NULL);
            SendMessage(g_hBtnReport, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            g_hBtnRefresh = CreateWindowA("BUTTON", "Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                         585, btnY, 85, 32, hWnd, (HMENU)ID_BTN_REFRESH, g_hInstance, NULL);
            SendMessage(g_hBtnRefresh, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            /* ListView in Report View */
            g_hListView = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
                                         WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER,
                                         15, 190, 1010, 440, hWnd, (HMENU)ID_LISTVIEW, g_hInstance, NULL);
            SendMessage(g_hListView, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            ListView_SetExtendedListViewStyle(g_hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

            /* Add Columns */
            LVCOLUMNA lvc = {0};
            lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT | LVCF_SUBITEM;

            lvc.fmt = LVCFMT_RIGHT; lvc.cx = 65; lvc.pszText = "ID"; ListView_InsertColumn(g_hListView, 0, &lvc);
            lvc.fmt = LVCFMT_LEFT;  lvc.cx = 175; lvc.pszText = "Product Name"; ListView_InsertColumn(g_hListView, 1, &lvc);
            lvc.fmt = LVCFMT_LEFT;  lvc.cx = 125; lvc.pszText = "Category"; ListView_InsertColumn(g_hListView, 2, &lvc);
            lvc.fmt = LVCFMT_RIGHT; lvc.cx = 75;  lvc.pszText = "Qty"; ListView_InsertColumn(g_hListView, 3, &lvc);
            lvc.fmt = LVCFMT_RIGHT; lvc.cx = 90;  lvc.pszText = "Price (Rs.)"; ListView_InsertColumn(g_hListView, 4, &lvc);
            lvc.fmt = LVCFMT_RIGHT; lvc.cx = 105; lvc.pszText = "Total (Rs.)"; ListView_InsertColumn(g_hListView, 5, &lvc);
            lvc.fmt = LVCFMT_RIGHT; lvc.cx = 80;  lvc.pszText = "Min Stock"; ListView_InsertColumn(g_hListView, 6, &lvc);
            lvc.fmt = LVCFMT_CENTER;lvc.cx = 100; lvc.pszText = "Expiry Date"; ListView_InsertColumn(g_hListView, 7, &lvc);
            lvc.fmt = LVCFMT_RIGHT; lvc.cx = 85;  lvc.pszText = "Days Left"; ListView_InsertColumn(g_hListView, 8, &lvc);
            lvc.fmt = LVCFMT_LEFT;  lvc.cx = 125; lvc.pszText = "Status"; ListView_InsertColumn(g_hListView, 9, &lvc);

            /* Load initial data */
            loadProducts();
            updateSummaryMetrics();
            refreshListView();
            return 0;
        }

        case WM_SIZE: {
            int cx = LOWORD(lParam);
            int cy = HIWORD(lParam);

            /* Resize Status Bar */
            SendMessage(g_hStatusBar, WM_SIZE, 0, 0);
            RECT rcStatus;
            GetWindowRect(g_hStatusBar, &rcStatus);
            int statusHeight = rcStatus.bottom - rcStatus.top;

            /* Resize ListView to fit window */
            int lvY = 190;
            int lvMargin = 15;
            int lvWidth = cx - (lvMargin * 2);
            int lvHeight = cy - lvY - statusHeight - 10;
            if (lvHeight < 100) lvHeight = 100;

            SetWindowPos(g_hListView, NULL, lvMargin, lvY, lvWidth, lvHeight, SWP_NOZORDER);

            /* Invalidate top header cards */
            RECT topRect = { 0, 0, cx, 105 };
            InvalidateRect(hWnd, &topRect, TRUE);
            return 0;
        }

        case WM_GETMINMAXINFO: {
            LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
            lpMMI->ptMinTrackSize.x = 880;
            lpMMI->ptMinTrackSize.y = 560;
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            RECT clientRect;
            GetClientRect(hWnd, &clientRect);

            RECT topCardsRect = { clientRect.left, clientRect.top, clientRect.right, 105 };
            drawSummaryCards(hdc, topCardsRect);

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_NOTIFY: {
            LPNMHDR pnm = (LPNMHDR)lParam;

            if (pnm->idFrom == ID_LISTVIEW) {
                /* Column Header Click -> Sort */
                if (pnm->code == LVN_COLUMNCLICK) {
                    LPNMLISTVIEW pnmlv = (LPNMLISTVIEW)lParam;
                    if (g_sortColumn == pnmlv->iSubItem) {
                        g_sortAscending = !g_sortAscending;
                    } else {
                        g_sortColumn = pnmlv->iSubItem;
                        g_sortAscending = 1;
                    }
                    refreshListView();
                    return 0;
                }

                /* Double click -> Edit or Quick Stock */
                if (pnm->code == NM_DBLCLK) {
                    int realIdx = getSelectedProductRealIndex();
                    if (realIdx != -1) {
                        showStockDialog(hWnd, realIdx);
                    }
                    return 0;
                }

                /* Custom Draw for colored rows & status badges */
                if (pnm->code == NM_CUSTOMDRAW) {
                    LPNMLVCUSTOMDRAW lplvcd = (LPNMLVCUSTOMDRAW)lParam;
                    switch (lplvcd->nmcd.dwDrawStage) {
                        case CDDS_PREPAINT:
                            return CDRF_NOTIFYITEMDRAW;

                        case CDDS_ITEMPREPAINT: {
                            int realIdx = (int)lplvcd->nmcd.lItemlParam;
                            if (realIdx >= 0 && realIdx < productCount) {
                                Product *p = &products[realIdx];
                                int days = daysUntilExpiry(p);
                                if (days < 0) {
                                    /* Expired: soft red background tint */
                                    lplvcd->clrTextBk = RGB(254, 242, 242);
                                    lplvcd->clrText = RGB(153, 27, 27);
                                } else if (days <= 30) {
                                    /* Expiring soon: soft orange tint */
                                    lplvcd->clrTextBk = RGB(255, 251, 235);
                                    lplvcd->clrText = RGB(146, 64, 14);
                                } else if (p->quantity <= p->minStock) {
                                    /* Low stock: soft yellow tint */
                                    lplvcd->clrTextBk = RGB(254, 249, 195);
                                    lplvcd->clrText = RGB(133, 77, 14);
                                } else if (lplvcd->nmcd.dwItemSpec % 2 == 1) {
                                    /* Alternating soft zebra stripe */
                                    lplvcd->clrTextBk = RGB(249, 250, 251);
                                    lplvcd->clrText = RGB(17, 24, 39);
                                } else {
                                    lplvcd->clrTextBk = RGB(255, 255, 255);
                                    lplvcd->clrText = RGB(17, 24, 39);
                                }
                            }
                            return CDRF_NEWFONT;
                        }
                    }
                    return CDRF_DODEFAULT;
                }
            }
            break;
        }

        case WM_CONTEXTMENU: {
            HWND hTarget = (HWND)wParam;
            if (hTarget == g_hListView) {
                int realIdx = getSelectedProductRealIndex();
                if (realIdx != -1) {
                    HMENU hMenu = CreatePopupMenu();
                    AppendMenuA(hMenu, MF_STRING, IDM_STOCK, "Quick Adjust Stock...");
                    AppendMenuA(hMenu, MF_STRING, IDM_EDIT, "Edit Product Details...");
                    AppendMenuA(hMenu, MF_SEPARATOR, 0, NULL);
                    AppendMenuA(hMenu, MF_STRING, IDM_DELETE, "Delete Product");
                    AppendMenuA(hMenu, MF_SEPARATOR, 0, NULL);
                    AppendMenuA(hMenu, MF_STRING, IDM_REPORT, "View Inventory Report");

                    POINT pt = { LOWORD(lParam), HIWORD(lParam) };
                    if (pt.x == -1 && pt.y == -1) {
                        RECT rcItem;
                        int sel = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
                        ListView_GetItemRect(g_hListView, sel, &rcItem, LVIR_BOUNDS);
                        pt.x = rcItem.left; pt.y = rcItem.bottom;
                        ClientToScreen(g_hListView, &pt);
                    }
                    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
                    DestroyMenu(hMenu);
                }
                return 0;
            }
            break;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);

            /* Search box live filtering */
            if (id == ID_SEARCH_EDIT && code == EN_CHANGE) {
                refreshListView();
                return 0;
            }

            /* Filter combo selection changed */
            if (id == ID_FILTER_COMBO && code == CBN_SELCHANGE) {
                refreshListView();
                return 0;
            }

            /* Clear Search */
            if (id == ID_BTN_CLEAR_SEARCH) {
                SetWindowTextA(g_hSearchEdit, "");
                SendMessage(g_hFilterCombo, CB_SETCURSEL, 0, 0);
                refreshListView();
                return 0;
            }

            /* Refresh */
            if (id == ID_BTN_REFRESH) {
                loadProducts();
                updateSummaryMetrics();
                refreshListView();
                InvalidateRect(hWnd, NULL, TRUE);
                return 0;
            }

            /* Add Product */
            if (id == ID_BTN_ADD) {
                if (productCount >= MAX_PRODUCTS) {
                    MessageBoxA(hWnd, "Inventory is full. Maximum 1000 products allowed.", "Limit Reached", MB_ICONWARNING);
                    return 0;
                }
                Product newProd = {0};
                if (showProductDialog(hWnd, 0, &newProd)) {
                    products[productCount++] = newProd;
                    saveProducts();
                    generateReportFile();
                    updateSummaryMetrics();
                    refreshListView();
                    InvalidateRect(hWnd, NULL, TRUE);
                    MessageBoxA(hWnd, "Product added successfully!", "Success", MB_ICONINFORMATION);
                }
                return 0;
            }

            /* Edit Product (button or menu) */
            if (id == ID_BTN_EDIT || id == IDM_EDIT) {
                int realIdx = getSelectedProductRealIndex();
                if (realIdx == -1) {
                    MessageBoxA(hWnd, "Please select a product from the list to edit.", "No Selection", MB_ICONINFORMATION);
                    return 0;
                }
                Product editProd = products[realIdx];
                if (showProductDialog(hWnd, 1, &editProd)) {
                    products[realIdx] = editProd;
                    saveProducts();
                    generateReportFile();
                    updateSummaryMetrics();
                    refreshListView();
                    InvalidateRect(hWnd, NULL, TRUE);
                    MessageBoxA(hWnd, "Product updated successfully!", "Success", MB_ICONINFORMATION);
                }
                return 0;
            }

            /* Update Stock (button or menu) */
            if (id == ID_BTN_STOCK || id == IDM_STOCK) {
                int realIdx = getSelectedProductRealIndex();
                if (realIdx == -1) {
                    MessageBoxA(hWnd, "Please select a product to adjust stock.", "No Selection", MB_ICONINFORMATION);
                    return 0;
                }
                showStockDialog(hWnd, realIdx);
                return 0;
            }

            /* Delete Product (button or menu) */
            if (id == ID_BTN_DELETE || id == IDM_DELETE) {
                int realIdx = getSelectedProductRealIndex();
                if (realIdx == -1) {
                    MessageBoxA(hWnd, "Please select a product to delete.", "No Selection", MB_ICONINFORMATION);
                    return 0;
                }
                char confirmMsg[256];
                sprintf(confirmMsg, "Are you sure you want to delete '%s' (ID: %d)?\nThis action cannot be undone.",
                        products[realIdx].name, products[realIdx].id);
                if (MessageBoxA(hWnd, confirmMsg, "Confirm Deletion", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                    for (int i = realIdx; i < productCount - 1; i++) {
                        products[i] = products[i + 1];
                    }
                    productCount--;
                    saveProducts();
                    generateReportFile();
                    updateSummaryMetrics();
                    refreshListView();
                    InvalidateRect(hWnd, NULL, TRUE);
                    MessageBoxA(hWnd, "Product deleted successfully.", "Deleted", MB_ICONINFORMATION);
                }
                return 0;
            }

            /* Generate & View Report */
            if (id == ID_BTN_REPORT || id == IDM_REPORT) {
                showReportViewer(hWnd);
                return 0;
            }
            break;
        }

        case WM_DESTROY:
            if (g_hFontNormal) DeleteObject(g_hFontNormal);
            if (g_hFontBold) DeleteObject(g_hFontBold);
            if (g_hFontLargeBold) DeleteObject(g_hFontLargeBold);
            if (g_hFontTitle) DeleteObject(g_hFontTitle);
            if (g_hFontFixed) DeleteObject(g_hFontFixed);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

/* WinMain entry point */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    g_hInstance = hInstance;

    /* High DPI support if available */
    typedef BOOL(WINAPI *SetProcessDPIAwareFunc)(void);
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (hUser32) {
        SetProcessDPIAwareFunc setDpi = (SetProcessDPIAwareFunc)GetProcAddress(hUser32, "SetProcessDPIAware");
        if (setDpi) setDpi();
    }

    createFolder("data");
    createFolder("reports");

    /* Initialize Common Controls */
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    /* Register Window Class */
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "SmartInventoryMainWindow";
    RegisterClassExA(&wc);

    /* Center window on screen */
    int w = 1060, h = 720;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - w) / 2;
    int y = (screenH - h) / 2;
    if (x < 10) x = 10;
    if (y < 10) y = 10;

    g_hMainWnd = CreateWindowExA(WS_EX_APPWINDOW, "SmartInventoryMainWindow",
                                "Smart Inventory Management System - GUI Edition",
                                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                x, y, w, h, NULL, NULL, hInstance, NULL);

    if (!g_hMainWnd) {
        char err[128];
        sprintf(err, "CreateWindow failed with error code: %lu", GetLastError());
        MessageBoxA(NULL, err, "Error", MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return WinMain(GetModuleHandleA(NULL), NULL, GetCommandLineA(), SW_SHOWNORMAL);
}
