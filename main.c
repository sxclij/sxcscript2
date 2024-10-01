#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define sxcscript_path "test/6.txt"
#define sxcscript_capacity (1 << 16)
#define sxcscript_buf_capacity (1 << 10)

enum bool {
    false = 0,
    true = 1,
};
enum sxcscript_kind {
    sxcscript_kind_null,
    sxcscript_kind_nop,
    sxcscript_kind_push,
    sxcscript_kind_label,
    sxcscript_kind_call,
    sxcscript_kind_jmp,
    sxcscript_kind_jze,
    sxcscript_kind_const_get,
    sxcscript_kind_const_set,
    sxcscript_kind_local_get,
    sxcscript_kind_local_set,
    sxcscript_kind_add,
    sxcscript_kind_sub,
    sxcscript_kind_mul,
    sxcscript_kind_div,
    sxcscript_kind_mod,
};
enum sxcscript_global {
    sxcscript_global_ip,
    sxcscript_global_sp,
    sxcscript_global_bp,
};
struct sxcscript_token {
    const char* data;
    int32_t size;
};
union sxcscript_node_val {
    int32_t label_i;
    struct sxcscript_node* label_break;
    struct sxcscript_node* label_continue;
    int32_t literal;
};
struct sxcscript_node {
    enum sxcscript_kind kind;
    struct sxcscript_token* token;
    union sxcscript_node_val val;
    struct sxcscript_node* prev;
    struct sxcscript_node* next;
};
struct sxcscript_label {
    struct sxcscript_node* node;
    int32_t arg_size;
    int32_t inst_i;
};
union sxcscript_inst {
    enum sxcscript_kind kind;
    int32_t val;
};
struct sxcscript {
    int32_t mem[sxcscript_capacity];
    union sxcscript_inst inst[sxcscript_capacity];
    struct sxcscript_token token[sxcscript_capacity];
    struct sxcscript_node node[sxcscript_capacity];
    struct sxcscript_label label[sxcscript_capacity];
    struct sxcscript_node* free;
    struct sxcscript_node* parsed;
    struct sxcscript_node* var;
};

uint64_t xorshift(uint64_t* x) {
    *x ^= *x << 13;
    *x ^= *x >> 7;
    *x ^= *x << 17;
    return *x;
}
enum bool sxcscript_token_eq(struct sxcscript_token* a, struct sxcscript_token* b) {
    if (a->size != b->size) {
        return false;
    }
    for (int i = 0; i < a->size; i++) {
        if (a->data[i] != b->data[i]) {
            return false;
        }
    }
    return true;
}
enum bool sxcscript_token_eq_str(struct sxcscript_token* a, const char* b) {
    for (int i = 0; i < a->size; i++) {
        if (a->data[i] != b[i] || b[i] == '\0') {
            return false;
        }
    }
    return true;
}
int32_t sxcscript_token_to_int32(struct sxcscript_token* token) {
    enum bool is_neg = token->data[0] == '-';
    int i = is_neg ? 1 : 0;
    int32_t ret = 0;
    for (; i < token->size; i++) {
        ret = (ret * 10) + token->data[i] - '0';
    }
    return is_neg ? -ret : ret;
}
void sxcscript_node_free(struct sxcscript_node** free, struct sxcscript_node* node) {
    node->prev = *free;
    (*free)->next = node;
    *free = node;
}
struct sxcscript_node* sxcscript_node_alloc(struct sxcscript_node** free) {
    struct sxcscript_node* node = *free;
    *free = (*free)->prev;
    *node = (struct sxcscript_node){.prev = NULL, .next = NULL};
    return node;
}
struct sxcscript_node* sxcscript_node_insert(struct sxcscript_node** free, struct sxcscript_node* next) {
    struct sxcscript_node* node = sxcscript_node_alloc(free);
    struct sxcscript_node* prev = next->prev;
    next->prev = node;
    *node = (struct sxcscript_node){.prev = prev, .next = next};
    if (prev != NULL) {
        prev->next = node;
    }
    return node;
}
struct sxcscript_node* sxcscript_node_find(struct sxcscript_node* src, struct sxcscript_node* this) {
    for (struct sxcscript_node* itr = src->prev; itr != NULL; itr = itr->prev) {
        if (itr->token == this->token) {
            return itr;
        }
    }
    for (struct sxcscript_node* itr = src->prev; itr != NULL; itr = itr->prev) {
        if (sxcscript_token_eq(itr->token, this->token)) {
            return itr;
        }
    }
    return NULL;
}
int32_t sxcscript_node_left(struct sxcscript_node* node) {
    int32_t ret = 0;
    for (struct sxcscript_node* itr = node; itr->prev != NULL; itr = itr->prev) {
        ret++;
    }
    return ret;
}
void sxcscript_node_init(struct sxcscript* sxcscript) {
    sxcscript->free = sxcscript->node;
    *(sxcscript->free) = (struct sxcscript_node){.prev = NULL, .next = NULL};
    for (int i = 1; i < sxcscript_capacity; i++) {
        sxcscript_node_free(&sxcscript->free, &sxcscript->node[i]);
    }
    sxcscript->parsed = sxcscript_node_alloc(&sxcscript->free);
    sxcscript->var = sxcscript_node_alloc(&sxcscript->free);
}
void sxcscript_tokenize(const char* src, struct sxcscript_token* token) {
    struct sxcscript_token* token_itr = token;
    *token_itr = (struct sxcscript_token){src, 0};
    for (const char* src_itr = src; *src_itr != '\0'; src_itr++) {
        if (*src_itr == ' ' || *src_itr == '\n') {
            if (token_itr->size != 0) {
                token_itr++;
            }
        } else if (*src_itr == '(' || *src_itr == ')' || *src_itr == ',' || *src_itr == '.') {
            if (token_itr->size != 0) {
                token_itr++;
            }
            *(token_itr++) = (struct sxcscript_token){src_itr, 1};
        } else {
            if (token_itr->size == 0) {
                token_itr->data = src_itr;
            }
            token_itr->size++;
        }
    }
}
void sxcscript_parse_push(struct sxcscript_node** free, struct sxcscript_node* parsed, enum sxcscript_kind kind, struct sxcscript_token* token) {
    struct sxcscript_node* node = sxcscript_node_insert(free, parsed);
    *node = (struct sxcscript_node){.kind = kind, .token = token, .prev = node->prev, .next = node->next};
}
void sxcscript_parse_expr(struct sxcscript* sxcscript, struct sxcscript_token** token_itr) {
    struct sxcscript_token* token_this = *token_itr;
    struct sxcscript_node* node_this = sxcscript->parsed;
    if (sxcscript_token_eq_str(token_this, "(")) {
        (*token_itr)++;
        while (!sxcscript_token_eq_str(*token_itr, ")")) {
            sxcscript_parse_expr(sxcscript, token_itr);
            if (sxcscript_token_eq_str(*token_itr, ",")) {
                (*token_itr)++;
            }
        }
        (*token_itr)++;
    } else if (sxcscript_token_eq_str(token_this, ".")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr);
    } else if (sxcscript_token_eq_str(*token_itr + 1, "(")) {
        (*token_itr)++;
        sxcscript_parse_expr(sxcscript, token_itr);
        sxcscript_parse_push(&sxcscript->free, sxcscript->parsed, sxcscript_kind_call, token_this);
    } else {
        sxcscript_parse_push(&sxcscript->free, sxcscript->parsed, sxcscript_kind_const_get, token_this);
        (*token_itr)++;
    }
}
void sxcscript_parse(struct sxcscript* sxcscript) {
    struct sxcscript_token* token_itr = sxcscript->token;
    sxcscript_parse_expr(sxcscript, &token_itr);
}
void sxcscript_analyze_primitive(struct sxcscript_node* parsed_begin) {
    for (struct sxcscript_node* parsed_itr = parsed_begin; parsed_itr != NULL; parsed_itr = parsed_itr->next) {
        if (sxcscript_token_eq_str(parsed_itr->token, "local_get")) {
            parsed_itr->kind = sxcscript_kind_local_get;
        } else if (sxcscript_token_eq_str(parsed_itr->token, "local_set")) {
            parsed_itr->kind = sxcscript_kind_local_set;
        } else if (sxcscript_token_eq_str(parsed_itr->token, "label")) {
            parsed_itr->kind = sxcscript_kind_label;
        } else if (sxcscript_token_eq_str(parsed_itr->token, "jmp")) {
            parsed_itr->kind = sxcscript_kind_jmp;
        } else if (sxcscript_token_eq_str(parsed_itr->token, "jze")) {
            parsed_itr->kind = sxcscript_kind_jze;
        }
    }
}
void sxcscript_analyze_var(struct sxcscript_node* parsed_begin) {
    struct {
        struct sxcscript_token* token;
        int32_t offset;
    } local[sxcscript_buf_capacity];
    int32_t offset_i = 0;
    int32_t local_i = 0;
    for (struct sxcscript_node* parsed_itr = parsed_begin; parsed_itr != NULL; parsed_itr = parsed_itr->next) {
        if (parsed_itr->kind == sxcscript_kind_const_get) {
            if ('0' <= parsed_itr->token->data[0] && parsed_itr->token->data[0] <= '9' || parsed_itr->token->data[0] == '-') {
                parsed_itr->val.literal = sxcscript_token_to_int32(parsed_itr->token);
                continue;
            }
            for (int i = 0;; i++) {
                if (sxcscript_token_eq_str(local[i].token, parsed_itr->token)) {
                    parsed_itr->val.literal = local[i].offset;
                    break;
                }
                if (local[i].token == NULL) {
                    local[i].token = parsed_itr->token;
                    local[i].offset = offset_i++;
                    parsed_itr->val.literal = local[i].offset;
                    break;
                }
            }
        }
    }
}
void sxcscript_analyze(struct sxcscript* sxcscript) {
    struct sxcscript_node* parsed_begin = sxcscript->parsed;
    while (parsed_begin->prev != NULL) {
        parsed_begin = parsed_begin->prev;
    }
    sxcscript_analyze_primitive(parsed_begin);
    sxcscript_analyze_var(parsed_begin);
    sxcscript_analyze_toinst(sxcscript);
}
void sxcscript_init(struct sxcscript* sxcscript, const char* src) {
    sxcscript_node_init(sxcscript);
    sxcscript_tokenize(src, sxcscript->token);
    sxcscript_parse(sxcscript);
    sxcscript_analyze(sxcscript);
}
void sxcscript_exec(struct sxcscript* sxcscript) {
    int32_t* ip = &(sxcscript->mem[sxcscript_global_ip]);
    int32_t* sp = &(sxcscript->mem[sxcscript_global_sp]);
    int32_t* bp = &(sxcscript->mem[sxcscript_global_bp]);
    *ip = 0;
    *sp = 256;
    *bp = 128;
    while (sxcscript->inst[*ip].kind != sxcscript_kind_null) {
        switch (sxcscript->inst[*ip].kind) {
            case sxcscript_kind_const_get:
                sxcscript->mem[(*sp)++] = sxcscript->inst[*ip].val;
                break;
            case sxcscript_kind_local_get:
                sxcscript->mem[(*sp)++] = sxcscript->mem[*bp + sxcscript->mem[*sp - 1]];
                break;
            case sxcscript_kind_local_set:
                sxcscript->mem[*bp + sxcscript->mem[*sp - 2]] = sxcscript->mem[*sp - 1];
                *sp -= 2;
                break;
        }
        (*ip)++;
    }
}
int main() {
    char src[sxcscript_capacity];
    static struct sxcscript sxcscript;

    int fd = open(sxcscript_path, O_RDONLY);
    int src_n = read(fd, src, sizeof(src) - 1);
    src[src_n] = '\0';
    close(fd);
    write(STDOUT_FILENO, src, src_n);
    write(STDOUT_FILENO, "\n", 1);

    sxcscript_init(&sxcscript, src);

    sxcscript_exec(&sxcscript);

    return 0;
}