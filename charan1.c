#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define ORDER 6
#define MAX_TRANSACTIONS 1000
#define MAX_BUYERS 100
#define MAX_SELLERS 100
#define ENERGY_THRESHOLD 300.0  

// ---------------------------- Structures ----------------------------

typedef struct Transaction {
    int transaction_id;
    int buyer_id;
    int seller_id;
    float energy_kwh;
    float price_per_kwh;
    float total_price;
    char datetime[20]; // Format: "YYYY-MM-DD HH:MM"
    float rate_below_300; 
    float rate_above_300;  
} Transaction;

typedef struct node {
    void **pointers;
    int *keys;
    struct node *parent;
    bool is_leaf;
    int num_keys;
    struct node *next;
} node;

typedef struct SellerKey {
    int seller_id;
    float rate_below_300;
    float rate_above_300;
    int regular_buyers[MAX_BUYERS];
    int regular_buyer_count;
    node *transaction_tree;
    int transaction_count;
} SellerKey;

typedef struct BuyerKey {
    int buyer_id;
    float total_energy_purchased;
    node *transaction_tree;
    int transaction_count;
} BuyerKey;
// ---------------------------- Globals ----------------------------


node *seller_tree = NULL;
node *buyer_tree = NULL;
node *global_transaction_tree=NULL;
Transaction *all_transactions[MAX_TRANSACTIONS];
int transaction_index=0;

// ---------------------------- B+ Tree Helpers ----------------------------

node *create_node(bool is_leaf) {
    node *new_node=(node *)malloc(sizeof(node));
    if (!new_node) {
        printf("Memory allocation failed for node.\n");
        exit(1);
    }
    new_node->pointers=(void **)malloc((ORDER + 1) * sizeof(void *));
    new_node->keys=(int *)malloc((ORDER - 1) * sizeof(int));

    if (!new_node->pointers || !new_node->keys) {
        printf("Memory allocation failed for node components.\n");
        exit(1);
    }

    new_node->parent=NULL;
    new_node->is_leaf=is_leaf;
    new_node->num_keys=0;
    new_node->next=NULL;

    return new_node;
}

void split_child(node *x, int index) {
    node *y = (node *)x->pointers[index];
    node *z = create_node(y->is_leaf);
    z->parent = x;

    int mid = ORDER / 2;
    int j = 0;

    if (y->is_leaf) {
        for (int i = mid; i < y->num_keys; i++) {
            z->keys[j] = y->keys[i];
            z->pointers[j] = y->pointers[i];
            j++;
        }
        z->num_keys = y->num_keys - mid;
        y->num_keys = mid;

        z->next = y->next;
        y->next = z;

        for (int i = x->num_keys + 1; i > index + 1; i--) {
            x->pointers[i] = x->pointers[i - 1];
        }
        for (int i = x->num_keys; i > index; i--) {
            x->keys[i] = x->keys[i - 1];
        }

        x->keys[index] = z->keys[0];  // Copy first key from z
        x->pointers[index + 1] = z;
    } else {
        for (int i = mid + 1; i < y->num_keys; i++) {
            z->keys[j] = y->keys[i];
            z->pointers[j] = y->pointers[i];
            j++;
        }
        z->pointers[j] = y->pointers[y->num_keys];

        z->num_keys = y->num_keys - mid - 1;
        y->num_keys = mid;

        for (int i = x->num_keys + 1; i > index + 1; i--) {
            x->pointers[i] = x->pointers[i - 1];
        }
        for (int i = x->num_keys; i > index; i--) {
            x->keys[i] = x->keys[i - 1];
        }

        x->keys[index] = y->keys[mid];
        x->pointers[index + 1] = z;
    }

    x->num_keys++;
}

void insert_non_full(node *x, Transaction *t) {
    int i = x->num_keys - 1;

    if (x->is_leaf == true) {
        while (i >= 0 && t->transaction_id < x->keys[i]) {
            x->keys[i + 1] = x->keys[i];
            x->pointers[i + 1] = x->pointers[i];
            i = i - 1;
        }

        x->keys[i + 1] = t->transaction_id;
        x->pointers[i + 1] = t;
        x->num_keys = x->num_keys + 1;
    } else {
        while (i >= 0 && t->transaction_id < x->keys[i]) {
            i = i - 1;
        }
        i = i + 1;

        node *child = (node *)x->pointers[i];
        if (child->num_keys == ORDER - 1) {
            split_child(x, i);
            if (t->transaction_id > x->keys[i]) {
                i = i + 1;
            }
        }

        insert_non_full((node *)x->pointers[i], t);
    }
}

node *insert_transaction(node *root, Transaction *t) {
    if (root == NULL) {
        root = create_node(true);
    }

    if (root->num_keys == ORDER - 1) {
        node *new_root = create_node(false);
        new_root->pointers[0] = root;
        root->parent = new_root;
        split_child(new_root, 0);
        insert_non_full(new_root, t);
        return new_root;
    } else {
        insert_non_full(root, t);
    }

    return root;
}

node *find_leftmost_leaf(node *root) {
    while (root != NULL && root->is_leaf == false) {
        root = root->pointers[0];
    }
    return root;
}

// ---------------------------- Core Functions ----------------------------






// Leap year checker
bool is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// Helper to extract and convert a number safely from a string range
int extract_int(const char *str, int start, int len) {
    int value = 0;
    for (int i = 0; i < len; i++) {
        
        value = value * 10 + (str[start + i] - '0');
    }
    return value;
}

// Main validation function
bool validate_datetime(const char *datetime) {
    if (strlen(datetime) != 16) return false;
    if (datetime[4] != '-' || datetime[7] != '-' || datetime[10] != ' ' || datetime[13] != ':') 
        return false;

    int year   = extract_int(datetime, 0, 4);
    int month  = extract_int(datetime, 5, 2);
    int day    = extract_int(datetime, 8, 2);
    int hour   = extract_int(datetime, 11, 2);
    int minute = extract_int(datetime, 14, 2);

    if (year < 1) return false;                    // Year must be positive
    if (month < 1 || month > 12) return false;
    if (hour < 0 || hour > 23) return false;
    if (minute < 0 || minute > 59) return false;

    int days_in_month[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (is_leap_year(year)) days_in_month[1] = 29;

    if (day < 1 || day > days_in_month[month - 1]) return false;

    return true;
}

SellerKey* get_or_create_seller(int seller_id, float rate_below_300, float rate_above_300) {
    // Search in seller_tree
    node *leaf = find_leftmost_leaf(seller_tree);
    while (leaf) {
        for (int i = 0; i < leaf->num_keys; i++) {
            SellerKey *s = (SellerKey *)leaf->pointers[i];
            if (s->seller_id == seller_id) {
                // Update rates if provided
                if (rate_below_300 > 0) s->rate_below_300 = rate_below_300;
                if (rate_above_300 > 0) s->rate_above_300 = rate_above_300;
                return s;
            }
        }
        leaf = leaf->next;
    }

    // Seller not found - create new
    SellerKey *new_seller = (SellerKey *)malloc(sizeof(SellerKey));
    new_seller->seller_id = seller_id;
    new_seller->rate_below_300 = rate_below_300;
    new_seller->rate_above_300 = rate_above_300;
    new_seller->transaction_tree = NULL;
    new_seller->transaction_count = 0;
    new_seller->regular_buyer_count = 0;

    // Insert into seller_tree using seller_id as key
    seller_tree = insert_transaction(seller_tree, (Transaction *)new_seller);
    return new_seller;
}
BuyerKey* get_or_create_buyer(int buyer_id) {
    // Search in buyer_tree
    node *leaf = find_leftmost_leaf(buyer_tree);
    while (leaf) {
        for (int i = 0; i < leaf->num_keys; i++) {
            BuyerKey *b = (BuyerKey *)leaf->pointers[i];
            if (b->buyer_id == buyer_id) {
                return b;
            }
        }
        leaf = leaf->next;
    }

    // Buyer not found - create new
    BuyerKey *new_buyer = (BuyerKey *)malloc(sizeof(BuyerKey));
    new_buyer->buyer_id = buyer_id;
    new_buyer->total_energy_purchased = 0;
    new_buyer->transaction_tree = NULL;
    new_buyer->transaction_count = 0;

    // Insert into buyer_tree using buyer_id as key
    buyer_tree = insert_transaction(buyer_tree, (Transaction *)new_buyer);
    return new_buyer;
}
float calculate_price(SellerKey *s, float energy_kwh, int buyer_id) {
    float total_price = 0.0;
    
    // Check if buyer is a regular customer
    bool is_regular = false;
    for (int i = 0; i < s->regular_buyer_count; i++) {
        if (s->regular_buyers[i] == buyer_id) {
            is_regular = true;
            break;
        }
    }
    
    // Calculate price based on tiered rates
    if (energy_kwh <= ENERGY_THRESHOLD) {
        total_price = energy_kwh * s->rate_below_300;
    } else {
        total_price = ENERGY_THRESHOLD * s->rate_below_300;
        total_price += (energy_kwh - ENERGY_THRESHOLD) * s->rate_above_300;
    }
    
    // Apply 5% discount for regular customers
    if (is_regular) {
        total_price *= 0.95;
    }
    
    return total_price;
}
bool add_transaction(Transaction *t) {
    // ... existing duplicate check code ...
    bool isadded = true;
    node *leaf = find_leftmost_leaf(global_transaction_tree);
    while (leaf) {
        for (int i = 0; i < leaf->num_keys; i++) {
            Transaction *t1 = (Transaction *)leaf->pointers[i];
            if(t1->transaction_id==t->transaction_id) {
                printf("Transaction already exists. Try again!");
                isadded = false;
                return isadded;
            }
        }
        leaf = leaf->next;
    }
    all_transactions[transaction_index++] = t;
    global_transaction_tree = insert_transaction(global_transaction_tree, t);

    // Use the new B+ tree versions
    SellerKey *s = get_or_create_seller(t->seller_id, t->rate_below_300, t->rate_above_300);
    s->transaction_tree = insert_transaction(s->transaction_tree, t);
    s->transaction_count++;

    BuyerKey *b = get_or_create_buyer(t->buyer_id);
    b->transaction_tree = insert_transaction(b->transaction_tree, t);
    b->total_energy_purchased += t->energy_kwh;
    b->transaction_count++;

    if (b->transaction_count > 5) {
        bool already_added = false;
        for (int j = 0; j < s->regular_buyer_count; j++) {
            if (s->regular_buyers[j] == b->buyer_id) {
                already_added = true;
                break;
            }
        }
        if (!already_added && s->regular_buyer_count < MAX_BUYERS) {
            s->regular_buyers[s->regular_buyer_count++] = b->buyer_id;
            printf("Buyer %d is now a regular customer of Seller %d!\n", b->buyer_id, s->seller_id);
        }
    }
    return true;
}

void display_all_transactions() {
    printf("\nAll Transactions:\n");
    printf("%-5s %-8s %-8s %-12s %-12s %-12s %-20s\n", 
           "ID", "Buyer", "Seller", "Energy(kWh)", "Price/kWh", "Total($)", "Time");
    printf("-----------------------------------------------------------------------\n");
    
    node *leaf = find_leftmost_leaf(global_transaction_tree);
    while (leaf) {
        for (int i = 0; i < leaf->num_keys; i++) {
            Transaction *t = (Transaction *)leaf->pointers[i];
            printf("%-5d %-8d %-8d %-12.2f %-12.2f %-12.2f %s\n",
                   t->transaction_id, t->buyer_id, t->seller_id, 
                   t->energy_kwh, t->price_per_kwh, t->total_price, t->datetime);
        }
        leaf = leaf->next;
    }
}

void transactions_by_seller() {
    printf("\nTransactions by Seller:\n");
    
    node *seller_leaf = find_leftmost_leaf(seller_tree);
    while (seller_leaf) {
        for (int i = 0; i < seller_leaf->num_keys; i++) {
            SellerKey *s = (SellerKey *)seller_leaf->pointers[i];
            
            printf("\nSeller %d:\n", s->seller_id);
            printf("Rates: %.2f$/kWh (≤300kWh), %.2f$/kWh (>300kWh)\n", 
                   s->rate_below_300, s->rate_above_300);
            
            // Print transactions for this seller
            node *trans_leaf = find_leftmost_leaf(s->transaction_tree);
            while (trans_leaf) {
                for (int j = 0; j < trans_leaf->num_keys; j++) {
                    Transaction *t = (Transaction *)trans_leaf->pointers[j];
                    printf("Transaction ID: %d, Buyer: %d, Energy: %.2f kWh\n",
                           t->transaction_id, t->buyer_id, t->energy_kwh);
                }
                trans_leaf = trans_leaf->next;
            }
        }
        seller_leaf = seller_leaf->next;
    }
}

void transactions_by_buyer() {
    printf("\nTransactions by Buyer:\n");
    
    // Start at the leftmost leaf of the buyer tree
    node *buyer_leaf = find_leftmost_leaf(buyer_tree);
    
    while (buyer_leaf != NULL) {
        // Iterate through all keys in this leaf node
        for (int i = 0; i < buyer_leaf->num_keys; i++) {
            BuyerKey *b = (BuyerKey *)buyer_leaf->pointers[i];
            
            printf("\nBuyer %d (Total Energy: %.2f kWh, Transactions: %d):\n", 
                   b->buyer_id, b->total_energy_purchased, b->transaction_count);
            
            printf("%-5s %-8s %-12s %-12s %-12s %-20s\n", 
                   "ID", "Seller", "Energy(kWh)", "Price/kWh", "Total($)", "Time");
            printf("-----------------------------------------------------------------\n");
            
            // Now display all transactions for this buyer
            node *trans_leaf = find_leftmost_leaf(b->transaction_tree);
            while (trans_leaf != NULL) {
                for (int j = 0; j < trans_leaf->num_keys; j++) {
                    Transaction *t = (Transaction *)trans_leaf->pointers[j];
                    
                    printf("%-5d %-8d %-12.2f %-12.2f %-12.2f %s\n", 
                           t->transaction_id, t->seller_id, t->energy_kwh, 
                           t->price_per_kwh, t->total_price, t->datetime);
                }
                trans_leaf = trans_leaf->next;
            }
        }
        buyer_leaf = buyer_leaf->next;
    }
}

void total_revenue_by_seller() {
    printf("\nTotal Revenue by Seller:\n");
    printf("%-8s %-15s %-15s %-15s\n", 
           "Seller", "Revenue($)", "Energy(kWh)", "Transactions");
    printf("--------------------------------------------------\n");
    
    // Start at the leftmost leaf of the seller tree
    node *seller_leaf = find_leftmost_leaf(seller_tree);
    
    while (seller_leaf != NULL) {
        // Iterate through all seller keys in this leaf node
        for (int i = 0; i < seller_leaf->num_keys; i++) {
            SellerKey *s = (SellerKey *)seller_leaf->pointers[i];
            float revenue = 0.0;
            float total_energy = 0.0;
            
            // Calculate revenue and energy by traversing the seller's transaction tree
            node *trans_leaf = find_leftmost_leaf(s->transaction_tree);
            while (trans_leaf != NULL) {
                for (int j = 0; j < trans_leaf->num_keys; j++) {
                    Transaction *t = (Transaction *)trans_leaf->pointers[j];
                    revenue += t->total_price;
                    total_energy += t->energy_kwh;
                }
                trans_leaf = trans_leaf->next;
            }
            
            printf("%-8d %-15.2f %-15.2f %-15d\n", 
                   s->seller_id, revenue, total_energy, s->transaction_count);
        }
        seller_leaf = seller_leaf->next;
    }
}
void energy_range_transactions(float min_kwh, float max_kwh) {
    printf("\nTransactions in Energy Range %.2f - %.2f kWh:\n", min_kwh, max_kwh);
    printf("%-5s %-8s %-8s %-12s %-12s %-12s\n", 
           "ID", "Buyer", "Seller", "Energy(kWh)", "Price/kWh", "Total($)");
    printf("--------------------------------------------------------------\n");
    
    node *leaf = find_leftmost_leaf(global_transaction_tree);
    while (leaf) {
        for (int i = 0; i < leaf->num_keys; i++) {
            Transaction *t = (Transaction *)leaf->pointers[i];
            if (t->energy_kwh >= min_kwh && t->energy_kwh <= max_kwh) {
                printf("%-5d %-8d %-8d %-12.2f %-12.2f %-12.2f\n", 
                       t->transaction_id, t->buyer_id, t->seller_id, 
                       t->energy_kwh, t->price_per_kwh, t->total_price);
            }
        }
        leaf = leaf->next;
    }
}

void sort_buyers_by_energy() {
    printf("\nBuyers Sorted by Total Energy Purchased:\n");
    printf("%-8s %-15s %-15s\n", "Buyer", "Energy(kWh)", "Transactions");
    printf("----------------------------------------\n");

    // First, collect all buyers into an array for sorting
    BuyerKey *buyers[MAX_BUYERS];
    int buyer_count = 0;

    // Traverse the B+ tree to collect buyers
    node *leaf = find_leftmost_leaf(buyer_tree);
    while (leaf != NULL && buyer_count < MAX_BUYERS) {
        for (int i = 0; i < leaf->num_keys; i++) {
            buyers[buyer_count++] = (BuyerKey *)leaf->pointers[i];
            if (buyer_count >= MAX_BUYERS) break;
        }
        leaf = leaf->next;
    }

    // Bubble sort
    for (int i = 0; i < buyer_count - 1; i++) {
        for (int j = 0; j < buyer_count - i - 1; j++) {
            if (buyers[j]->total_energy_purchased > buyers[j+1]->total_energy_purchased) {
                BuyerKey *temp = buyers[j];
                buyers[j] = buyers[j+1];
                buyers[j+1] = temp;
            }
        }
    }

    // Display sorted results
    for (int i = 0; i < buyer_count; i++) {
        printf("%-8d %-15.2f %-15d\n", 
               buyers[i]->buyer_id, 
               buyers[i]->total_energy_purchased,
               buyers[i]->transaction_count);
    }
}

void sort_pairs_by_transaction_count() {
    printf("\nBuyer/Seller Pairs by Number of Transactions:\n");
    printf("%-8s %-8s %-15s\n", "Buyer", "Seller", "Transactions");
    printf("--------------------------------\n");

    // Structure to hold buyer-seller pairs
    typedef struct {
        int buyer_id;
        int seller_id;
        int transaction_count;
    } Pair;
    
    Pair pairs[MAX_BUYERS * MAX_SELLERS];
    int pair_count = 0;

    // Traverse buyer tree
    node *buyer_leaf = find_leftmost_leaf(buyer_tree);
    while (buyer_leaf != NULL && pair_count < MAX_BUYERS * MAX_SELLERS) {
        for (int b = 0; b < buyer_leaf->num_keys; b++) {
            BuyerKey *buyer = (BuyerKey *)buyer_leaf->pointers[b];
            
            // For each buyer, traverse their transactions
            node *trans_leaf = find_leftmost_leaf(buyer->transaction_tree);
            while (trans_leaf != NULL && pair_count < MAX_BUYERS * MAX_SELLERS) {
                for (int t = 0; t < trans_leaf->num_keys; t++) {
                    Transaction *trans = (Transaction *)trans_leaf->pointers[t];
                    
                    // Check if this pair already exists
                    bool found = false;
                    for (int p = 0; p < pair_count; p++) {
                        if (pairs[p].buyer_id == buyer->buyer_id && 
                            pairs[p].seller_id == trans->seller_id) {
                            pairs[p].transaction_count++;
                            found = true;
                            break;
                        }
                    }
                    
                    if (!found && pair_count < MAX_BUYERS * MAX_SELLERS) {
                        pairs[pair_count].buyer_id = buyer->buyer_id;
                        pairs[pair_count].seller_id = trans->seller_id;
                        pairs[pair_count].transaction_count = 1;
                        pair_count++;
                    }
                }
                trans_leaf = trans_leaf->next;
            }
        }
        buyer_leaf = buyer_leaf->next;
    }

    // Bubble sort (keeping original sorting approach)
    for (int i = 0; i < pair_count - 1; i++) {
        for (int j = 0; j < pair_count - i - 1; j++) {
            if (pairs[j].transaction_count < pairs[j+1].transaction_count) {
                Pair temp = pairs[j];
                pairs[j] = pairs[j+1];
                pairs[j+1] = temp;
            }
        }
    }

    // Display results
    for (int i = 0; i < pair_count; i++) {
        printf("%-8d %-8d %-15d\n", 
               pairs[i].buyer_id, 
               pairs[i].seller_id, 
               pairs[i].transaction_count);
    }
}

void transactions_in_time_range(const char *start_str, const char *end_str) {
    printf("\nTransactions from %s to %s:\n", start_str, end_str);
    printf("%-5s %-8s %-8s %-12s %-12s %-12s %-20s\n", 
           "ID", "Buyer", "Seller", "Energy(kWh)", "Price/kWh", "Total($)", "Time");
    printf("---------------------------------------------------------------------------------\n");
    
    node *leaf = find_leftmost_leaf(global_transaction_tree);
    while (leaf) {
        for (int i = 0; i < leaf->num_keys; i++) {
            Transaction *t = (Transaction *)leaf->pointers[i];
            if (strcmp(t->datetime, start_str) >= 0 && strcmp(t->datetime, end_str) <= 0) {
                printf("%-5d %-8d %-8d %-12.2f %-12.2f %-12.2f %s\n",
                       t->transaction_id, t->buyer_id, t->seller_id, 
                       t->energy_kwh, t->price_per_kwh, t->total_price, t->datetime);
            }
        }
        leaf = leaf->next;
    }
}

void save_transactions_to_file() {
    FILE *file = fopen("transactions.txt", "w");
    if (file == NULL) {
        printf("Error: Could not create transactions.txt\n");
        return;
    }

    fprintf(file, "# transaction_id,buyer_id,seller_id,energy_kwh,rate_below_300,rate_above_300,datetime\n");

    int saved_count = 0;
    node *leaf = find_leftmost_leaf(global_transaction_tree);

    while (leaf != NULL) {
        for (int i = 0; i < leaf->num_keys; i++) {
            if (leaf->pointers[i] == NULL) continue;

            Transaction *t = (Transaction *)leaf->pointers[i];
            fprintf(file, "%d,%d,%d,%.2f,%.2f,%.2f,%s\n",
                   t->transaction_id, t->buyer_id, t->seller_id,
                   t->energy_kwh, t->rate_below_300, t->rate_above_300,
                   t->datetime);
            saved_count++;
        }
        leaf = leaf->next;
    }

    

    fclose(file);
    
}

void load_transactions_from_file() {
    FILE *file = fopen("transactions.txt", "r");
    if (file == NULL) {
        printf("Info: No existing transaction file found. Starting fresh.\n");
        return;
    }

    printf("Loading transactions from file...\n");
    char line[256];
    int line_num = 0;
    int loaded_count = 0;
    int skipped_count = 0;
    bool loading_sellers = false;

    while (fgets(line, sizeof(line), file)) {
        line_num++;

        // Skip empty lines or comments
        if (line[0] == '\n' || line[0] == '#') {
            if (strstr(line, "# Sellers")) {
                loading_sellers = true;
            }
            continue;
        }

        // Remove newline character
        line[strcspn(line, "\n")] = '\0';

        if (!loading_sellers) {
            // Parse transaction line
            Transaction t;
            char datetime_str[20];
            int matched = sscanf(line, "%d,%d,%d,%f,%f,%f,%19[^,]", 
                               &t.transaction_id, &t.buyer_id, &t.seller_id,
                               &t.energy_kwh, &t.rate_below_300, &t.rate_above_300,
                               datetime_str);

            if (matched != 7) {
                printf("Line %d: Skipped transaction - Malformed (fields=%d)\n", line_num, matched);
                skipped_count++;
                continue;
            }

            if (!validate_datetime(datetime_str)) {
                printf("Line %d: Skipped - Invalid datetime format: %s\n", line_num, datetime_str);
                skipped_count++;
                continue;
            }

            strncpy(t.datetime, datetime_str, sizeof(t.datetime));

            // Check for duplicate transaction ID
            bool is_duplicate = false;
            node *check_leaf = find_leftmost_leaf(global_transaction_tree);
            while (check_leaf) {
                for (int i = 0; i < check_leaf->num_keys; i++) {
                    Transaction *existing = (Transaction *)check_leaf->pointers[i];
                    if (existing->transaction_id == t.transaction_id) {
                        is_duplicate = true;
                        break;
                    }
                }
                if (is_duplicate) break;
                check_leaf = check_leaf->next;
            }

            if (is_duplicate) {
                printf("Line %d: Skipped - Duplicate transaction ID %d\n", line_num, t.transaction_id);
                skipped_count++;
                continue;
            }

            // Create seller if needed
            SellerKey *s = get_or_create_seller(t.seller_id, t.rate_below_300, t.rate_above_300);

            // Calculate price
            if (t.energy_kwh <= ENERGY_THRESHOLD) {
                t.price_per_kwh = s->rate_below_300;
            } else {
                float below_price = ENERGY_THRESHOLD * s->rate_below_300;
                float above_price = (t.energy_kwh - ENERGY_THRESHOLD) * s->rate_above_300;
                t.price_per_kwh = (below_price + above_price) / t.energy_kwh;
            }
            t.total_price = calculate_price(s, t.energy_kwh, t.buyer_id);

            // Create transaction
            Transaction *new_t = (Transaction *)malloc(sizeof(Transaction));
            if (!new_t) {
                printf("Line %d: Error - Memory allocation failed\n", line_num);
                skipped_count++;
                continue;
            }
            *new_t = t;

            // Add to trees
            all_transactions[transaction_index++] = new_t;
            global_transaction_tree = insert_transaction(global_transaction_tree, new_t);
            
            s->transaction_tree = insert_transaction(s->transaction_tree, new_t);
            s->transaction_count++;

            BuyerKey *b = get_or_create_buyer(t.buyer_id);
            b->transaction_tree = insert_transaction(b->transaction_tree, new_t);
            b->total_energy_purchased += t.energy_kwh;
            b->transaction_count++;

            loaded_count++;
        } else {
            // Parse seller information
            int seller_id;
            float rate_below, rate_above;
            int regular_count;
            char *token = strtok(line, ",");
            
            if (token == NULL) continue;
            seller_id = atoi(token);
            
            token = strtok(NULL, ",");
            if (token == NULL) continue;
            rate_below = atof(token);
            
            token = strtok(NULL, ",");
            if (token == NULL) continue;
            rate_above = atof(token);
            
            token = strtok(NULL, ",");
            if (token == NULL) continue;
            regular_count = atoi(token);
            
            SellerKey *s = get_or_create_seller(seller_id, rate_below, rate_above);
            s->regular_buyer_count = regular_count;
            
            for (int i = 0; i < regular_count && i < MAX_BUYERS; i++) {
                token = strtok(NULL, ",");
                if (token == NULL) break;
                s->regular_buyers[i] = atoi(token);
            }
        }
    }

    fclose(file);
   
}
// ---------------------------- Main Menu ----------------------------

int main() {
    // Initialize global trees
    global_transaction_tree = NULL;
    seller_tree = NULL;
    buyer_tree = NULL;
    transaction_index = 0;

    // Load existing data
    load_transactions_from_file();

    int choice;
    do {
        printf("\n==== Energy Trading Record Management System ====\n");
        printf("1. Add New Transaction\n");
        printf("2. Display All Transactions\n");
        printf("3. Transactions for Every Seller\n");
        printf("4. Transactions for Every Buyer\n");
        printf("5. Total Revenue by Seller\n");
        printf("6. Transactions in Energy Range\n");
        printf("7. Sort Buyers by Energy Bought\n");
        printf("8. Sort Buyer/Seller Pairs\n");
        printf("9. Transactions in Time Range\n");
        printf("0. Exit\n");
        printf("Choice: ");
        scanf("%d", &choice);

        switch(choice) {
            case 1: {
                printf("\nEnter Transaction Details:\n");
                int transaction_id, buyer_id, seller_id;
                float energy_kwh, rate_below_300, rate_above_300;
                char datetime[20];
                
                printf("Transaction ID: ");
                scanf("%d", &transaction_id);
                printf("Buyer ID: ");
                scanf("%d", &buyer_id);
                printf("Seller ID: ");
                scanf("%d", &seller_id);
                printf("Energy (kWh): ");
                scanf("%f", &energy_kwh);
                
                // Check if seller exists to get existing rates
                SellerKey *existing_seller = NULL;
                node *seller_leaf = find_leftmost_leaf(seller_tree);
                while (seller_leaf) {
                    for (int i = 0; i < seller_leaf->num_keys; i++) {
                        SellerKey *s = (SellerKey *)seller_leaf->pointers[i];
                        if (s->seller_id == seller_id) {
                            existing_seller = s;
                            break;
                        }
                    }
                    if (existing_seller) break;
                    seller_leaf = seller_leaf->next;
                }
                
                if (existing_seller) {
                    rate_below_300 = existing_seller->rate_below_300;
                    rate_above_300 = existing_seller->rate_above_300;
                    printf("Using existing rates for Seller %d: %.2f$/kWh (≤300kWh), %.2f$/kWh (>300kWh)\n", 
                           seller_id, rate_below_300, rate_above_300);
                } else {
                    printf("Enter rate for energy <= 300 kWh ($/kWh): ");
                    scanf("%f", &rate_below_300);
                    printf("Enter rate for energy > 300 kWh ($/kWh): ");
                    scanf("%f", &rate_above_300);
                }
                
                printf("Enter date and time (YYYY-MM-DD HH:MM): ");
                scanf(" %19[^\n]", datetime);
                while(!validate_datetime(datetime)) {
                    printf("Invalid datetime format.\n");
                    printf("Enter date and time (YYYY-MM-DD HH:MM): ");
                    scanf(" %19[^\n]", datetime);
                    
                }
                
                Transaction *t = (Transaction *)malloc(sizeof(Transaction));
                t->transaction_id = transaction_id;
                t->buyer_id = buyer_id;
                t->seller_id = seller_id;
                t->energy_kwh = energy_kwh;
                strncpy(t->datetime, datetime, sizeof(t->datetime));
                t->rate_below_300 = rate_below_300;
                t->rate_above_300 = rate_above_300;
                
                SellerKey *s = get_or_create_seller(seller_id, rate_below_300, rate_above_300);
                
                // Calculate price
                if (energy_kwh <= ENERGY_THRESHOLD) {
                    t->price_per_kwh = s->rate_below_300;
                } else {
                    t->price_per_kwh = s->rate_above_300;
                }
                
                t->total_price = calculate_price(s, energy_kwh, buyer_id);
                
                if (add_transaction(t)) {
                    save_transactions_to_file();
                    printf("Transaction added successfully!\n");
                }
                break;
            
            case 2:
                display_all_transactions();
                break;
            case 3:
                transactions_by_seller();
                break;
            case 4:
                transactions_by_buyer();
                break;
            case 5:
                total_revenue_by_seller();
                break;
            case 6: {
                float min, max;
                printf("\nEnter min energy (kWh): ");
                scanf("%f", &min);
                printf("Enter max energy (kWh): ");
                scanf("%f", &max);
                energy_range_transactions(min, max);
                break;
            }
            case 7:
                sort_buyers_by_energy();
                break;
            case 8:
                sort_pairs_by_transaction_count();
                break;
            case 9: {
                char start_str[20], end_str[20];
                bool isvalid = false;
                while(!isvalid) {
                    printf("Enter start time (YYYY-MM-DD HH:MM): ");
                    scanf(" %19[^\n]", start_str);
                    printf("Enter end time (YYYY-MM-DD HH:MM): ");
                    scanf(" %19[^\n]", end_str);
                    if (!validate_datetime(start_str) || !validate_datetime(end_str)) {
                        printf("Invalid datetime format. Try again.\n");
                    }
                    else isvalid = true;
                } 
                transactions_in_time_range(start_str, end_str);
                break;
            }
            case 0:
                printf("Exiting...\n");
                break;
            default:
                printf("Invalid choice. Please try again.\n");
        }
    }} while (choice != 0);

    

    return 0;
}


    