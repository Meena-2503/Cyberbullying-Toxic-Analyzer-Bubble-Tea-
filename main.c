#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_WORDS 200000
#define MAX_WORD_LEN 80
#define MAX_STOPWORDS 10000
#define MAX_TOXIC 10000
#define OUTPUT_FILE "analysis_report.txt"

typedef struct {
    char word[MAX_WORD_LEN];
    int count;
    int toxicity;
} WordInfo;

static WordInfo words[MAX_WORDS];
static int word_count = 0;

static char stopwords[MAX_STOPWORDS][MAX_WORD_LEN];
static int stopword_count = 0;

static char toxic_words[MAX_TOXIC][MAX_WORD_LEN];
static int toxic_level[MAX_TOXIC];
static int toxic_count = 0;

static int sentence_count = 0;
static int character_count = 0;

/* ---------------- CLEAN WORD ---------------- */
void clean_word(char *word) {
    int j = 0;
    char temp[MAX_WORD_LEN] = {0};
    for (int i = 0; word[i] && j < MAX_WORD_LEN - 1; i++) {
        if (isalpha((unsigned char)word[i]))
            temp[j++] = tolower((unsigned char)word[i]);
    }
    temp[j] = '\0';
    strcpy(word, temp);
}


/* ---------------- LOAD STOPWORDS ---------------- */
void load_stopwords() {
    FILE *fp = fopen("stopwords.txt", "r");
    if (!fp) {
        printf("Warning: stopwords.txt not found.\n");
        return;
    }

    char buf[270];
    while (fgets(buf, sizeof(buf), fp)) {
        char *p = buf;

        while (*p && isspace((unsigned char)*p)) p++;

        char *end = p + strlen(p) - 1;
        while (end >= p && isspace((unsigned char)*end)) {
            *end = '\0';
            end--;
        }

        if (*p == '\0') continue;

        clean_word(p);
        if (strlen(p) == 0) continue;

        if (stopword_count < MAX_STOPWORDS)
            strncpy(stopwords[stopword_count++], p, MAX_WORD_LEN - 1);
    }
    fclose(fp);
}


/* ---------------- LOAD TOXIC WORDS ---------------- */
void load_toxicwords() {
    FILE *fp = fopen("toxicwords.txt", "r");
    if (!fp) {
        printf("Warning: toxicwords.txt not found.\n");
        return;
    }

    char word[MAX_WORD_LEN];
    int severity;

    while (fscanf(fp, "%79s %d", word, &severity) == 2) {
        clean_word(word);
        if (strlen(word) == 0) continue;

        if (toxic_count < MAX_TOXIC) {
            strncpy(toxic_words[toxic_count], word, MAX_WORD_LEN - 1);
            toxic_level[toxic_count] = severity;
            toxic_count++;
        }
    }
    fclose(fp);
}


/* ---------------- CHECK STOPWORD ---------------- */
int is_stopword(const char *word) {
    for (int i = 0; i < stopword_count; i++)
        if (strcmp(word, stopwords[i]) == 0) return 1;
    return 0;
}


/* ---------------- GET TOXICITY ---------------- */
int get_toxicity(const char *word) {
    for (int i = 0; i < toxic_count; i++)
        if (strcmp(word, toxic_words[i]) == 0)
            return toxic_level[i];
    return 0;
}


/* ---------------- ADD WORD ---------------- */
void add_word(const char *word_in) {
    if (!word_in || strlen(word_in) == 0) return;

    char word[MAX_WORD_LEN];
    strncpy(word, word_in, MAX_WORD_LEN - 1);
    word[MAX_WORD_LEN - 1] = '\0';

    clean_word(word);
    if (strlen(word) == 0) return;
    if (is_stopword(word)) return;

    if (word_count >= MAX_WORDS) {
        fprintf(stderr, "Warning: MAX_WORDS limit reached.\n");
        return;
    }

    for (int i = 0; i < word_count; i++) {
        if (strcmp(words[i].word, word) == 0) {
            words[i].count++;
            return;
        }
    }

    strncpy(words[word_count].word, word, MAX_WORD_LEN - 1);
    words[word_count].count = 1;
    words[word_count].toxicity = get_toxicity(word);
    word_count++;
}


/* ---------------- CSV TWEET PARSER ---------------- */
static int parse_csv_tweet(char *line, char *out_tweet, int out_sz) {
    if (!line || !out_tweet) return 0;

    char *p = line;
    while (*p == ' ' || *p == '\t') p++;

    if (*p == '"') {
        p++;
        int i = 0;
        while (*p && !( *p == '"' && (*(p+1) == ',' || *(p+1) == '\0' || *(p+1) == '\n' ) )
               && i < out_sz - 1) {
            out_tweet[i++] = *p++;
        }
        out_tweet[i] = '\0';
        return 1;
    }

    int i = 0;
    while (*p && *p != ',' && *p != '\n' && i < out_sz - 1) {
        out_tweet[i++] = *p++;
    }
    out_tweet[i] = '\0';
    return 1;
}


/* ---------------- PROCESS FILE ---------------- */
void process_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("Cannot open %s\n", filename);
        return;
    }

    // Empty file detection
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);

    if (file_size == 0) {
        printf("Error: The file '%s' is empty.\n", filename);
        fclose(fp);
        return;
    }

    char line[4500];
    int line_no = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_no++;

        char tweet[3000] = {0};
        if (!parse_csv_tweet(line, tweet, sizeof(tweet))) continue;

        char token[MAX_WORD_LEN];
        int pos = 0;

        for (int i = 0; tweet[i]; i++) {

            /* Count characters for average word length */
            if (isalpha((unsigned char)tweet[i])) {
                character_count++;
            }

            /* Detect sentence endings . ? ! */
            if (tweet[i] == '.' || tweet[i] == '?' || tweet[i] == '!') {
                sentence_count++;
            }

            /* Existing tokenization logic */
            if (isalpha((unsigned char)tweet[i])) {
                if (pos < MAX_WORD_LEN - 1)
                    token[pos++] = tweet[i];
                } else {
                if (pos > 0) {
                    token[pos] = '\0';
                    add_word(token);
                    pos = 0;
                }
            }
        }

        if (pos > 0) {
            token[pos] = '\0';
            add_word(token);
        }
    }

    fclose(fp);

    printf("File processed. Total lines: %d\n", line_no);
}



/* ---------------- SORT BY FREQUENCY ---------------- */
void sort_by_frequency() {
    for (int i = 0; i < word_count - 1; i++) {
        for (int j = 0; j < word_count - i - 1; j++) {
            if (words[j].count < words[j+1].count) {
                WordInfo temp = words[j];
                words[j] = words[j+1];
                words[j+1] = temp;
            }
        }
    }
}


/* ---------------- SHOW STATISTICS ---------------- */
void show_statistics() {
    int total_words = 0;
    for (int i = 0; i < word_count; i++)
        total_words += words[i].count;

    float lexical_diversity =
        total_words ? (float)word_count / total_words : 0.0f;

    printf("\n===== GENERAL STATISTICS =====\n");
    printf("Total tokens: %d\n", total_words);
    printf("Unique words: %d\n", word_count);
    printf("Lexical diversity: %.3f\n", lexical_diversity);

    /* Avoid divide-by-zero errors */
    float avg_sentence_len = (sentence_count > 0)
        ? (float) total_words / sentence_count
        : 0.0f;

    float avg_word_len = (total_words > 0)
        ? (float) character_count / total_words
        : 0.0f;

    printf("Sentences detected: %d\n", sentence_count);
    printf("Average sentence length: %.2f words\n", avg_sentence_len);
    printf("Average word length: %.2f characters\n", avg_word_len);

}


/* ---------------- TOXIC ANALYSIS ---------------- */
void show_toxic_analysis() {
    int toxic_total = 0, toxic_unique = 0;

    for (int i = 0; i < word_count; i++) {
        if (words[i].toxicity > 0) {
            toxic_total += words[i].count;
            toxic_unique++;
        }
    }

    printf("\n===== TOXIC WORD ANALYSIS =====\n");
    printf("Unique toxic words: %d\n", toxic_unique);
    printf("Total toxic occurrences: %d\n\n", toxic_total);

    printf("Top toxic words:\n");
    sort_by_frequency();

    int shown = 0;
    for (int i = 0; i < word_count && shown < 10; i++) {
        if (words[i].toxicity > 0) {
            printf("%s (%d) [severity %d]\n",
                   words[i].word, words[i].count, words[i].toxicity);
            shown++;
        }
    }
}


/* ---------------- SHOW TOP N WORDS ---------------- */
void show_top_n_words() {
    if (word_count == 0) {
        printf("No words processed.\n");
        return;
    }

    sort_by_frequency();

    int N;
    printf("Enter how many top words to display: ");
    if (scanf("%d", &N) != 1) {
        printf("Invalid input.\n");
        return;
    }

    if (N > word_count) N = word_count;

    printf("\n===== TOP %d WORDS =====\n", N);
    printf("%-20s | %s\n", "WORD", "COUNT");
    printf("--------------------------------------\n");

    for (int i = 0; i < N; i++) {
        printf("%-20s | %d\n", words[i].word, words[i].count);
    }

}


/* ---------------- SAVE REPORT ---------------- */
void save_report() {
    FILE *fp = fopen(OUTPUT_FILE, "w");
    if (!fp) {
        printf("Cannot create output file.\n");
        return;
    }

    /* Compute statistics */
    int total_words = 0;
    for (int i = 0; i < word_count; i++)
        total_words += words[i].count;

    float lexical_diversity =
        total_words ? (float)word_count / total_words : 0.0f;

    float avg_sentence_len = (sentence_count > 0)
        ? (float)total_words / sentence_count
        : 0.0f;

    float avg_word_len = (total_words > 0)
        ? (float)character_count / total_words
        : 0.0f;

    /* Toxicity summary */
    int toxic_total = 0, toxic_unique = 0, max_severity = 0;
    for (int i = 0; i < word_count; i++) {
        if (words[i].toxicity > 0) {
            toxic_total += words[i].count;
            toxic_unique++;
            if (words[i].toxicity > max_severity)
                max_severity = words[i].toxicity;
        }
    }

    sort_by_frequency();

    fprintf(fp, "=============================================\n");
    fprintf(fp, "          TEXT ANALYSIS REPORT\n");
    fprintf(fp, "=============================================\n\n");

    /* GENERAL STATISTICS */
    fprintf(fp, "------------- GENERAL STATISTICS ------------\n");
    fprintf(fp, "Total tokens: %d\n", total_words);
    fprintf(fp, "Unique words: %d\n", word_count);
    fprintf(fp, "Lexical diversity: %.3f\n", lexical_diversity);
    fprintf(fp, "Sentences detected: %d\n", sentence_count);
    fprintf(fp, "Average sentence length: %.2f words\n", avg_sentence_len);
    fprintf(fp, "Average word length: %.2f characters\n\n", avg_word_len);

    /* TOXICITY SECTION */
    fprintf(fp, "------------- TOXICITY ANALYSIS -------------\n");
    fprintf(fp, "Unique toxic words: %d\n", toxic_unique);
    fprintf(fp, "Total toxic occurrences: %d\n", toxic_total);
    fprintf(fp, "Highest severity level: %d\n\n", max_severity);

    fprintf(fp, "Top 10 toxic words:\n");

    int shown = 0;
    for (int i = 0; i < word_count && shown < 10; i++) {
        if (words[i].toxicity > 0) {
            fprintf(fp, "%-15s %d (severity %d)\n",
                    words[i].word, words[i].count, words[i].toxicity);
            shown++;
        }
    }
    fprintf(fp, "\n");

    /* TOP WORDS */
    fprintf(fp, "------------ TOP 20 MOST FREQUENT WORDS -----------\n");
    for (int i = 0; i < 20 && i < word_count; i++) {
        fprintf(fp, "%-20s %d\n", words[i].word, words[i].count);
    }

    fprintf(fp, "\n=============================================\n");
    fprintf(fp, "End of Report\n");
    fprintf(fp, "=============================================\n");

    fclose(fp);
    printf("Report saved to %s.\n", OUTPUT_FILE);
}

/* ---------------- MAIN MENU ---------------- */
int main() {
    int choice;
    char filename[512];

    load_stopwords();
    load_toxicwords();

    while (1) {
        printf("\n===== CYBERBULLYING / TOXIC TEXT ANALYZER =====\n");
        printf("1. Load and analyze a text/CSV file\n");
        printf("2. Show general statistics\n");
        printf("3. Show toxic analysis\n");
        printf("4. Show top N words\n");
        printf("5. Save analysis report\n");
        printf("6. Exit\n");
        printf("Enter choice: ");

        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            printf("Invalid input.\n");
            continue;
        }

        switch (choice) {
            case 1:
                printf("Enter file name: ");
                scanf("%s", filename);
                process_file(filename);
                break;

            case 2:
                show_statistics();
                break;

            case 3:
                show_toxic_analysis();
                break;

            case 4:
                show_top_n_words();
                break;

            case 5:
                save_report();
                break;

            case 6:
                printf("Goodbye.\n");
                return 0;

            default:
                printf("Invalid choice.\n");
        }
    }

    return 0;
}