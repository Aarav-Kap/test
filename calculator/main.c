#include <libndls.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PROMPT_MAX 900
#define RESPONSE_MAX 2200
#define PAGE_SIZE 650

static unsigned long make_request_id(const char *prompt) {
    unsigned long h = (unsigned long)clock();
    const unsigned char *p = (const unsigned char *)prompt;
    while (*p) {
        h ^= (unsigned long)*p++;
        h *= 16777619UL;
    }
    return h ? h : 1UL;
}

static int write_request(unsigned long id, const char *prompt) {
    FILE *f = fopen("request.tns", "wb");
    if (!f) return 0;
    fprintf(f, "NSPIREAI_REQUEST_V1\nid=%lu\n%s\n", id, prompt);
    fclose(f);
    return 1;
}

static int read_response(unsigned long expected_id, char *out, size_t out_size) {
    FILE *f = fopen("response.tns", "rb");
    char header[64];
    char idline[64];
    unsigned long got_id = 0;
    size_t used = 0;

    if (!f) return 0;
    if (!fgets(header, sizeof(header), f)) { fclose(f); return 0; }
    header[strcspn(header, "\r\n")] = 0;
    if (strcmp(header, "NSPIREAI_RESPONSE_V1") != 0) { fclose(f); return 0; }

    if (!fgets(idline, sizeof(idline), f)) { fclose(f); return 0; }
    if (sscanf(idline, "id=%lu", &got_id) != 1) { fclose(f); return 0; }
    if (got_id != expected_id) { fclose(f); return -1; }

    while (!feof(f) && used + 1 < out_size) {
        size_t n = fread(out + used, 1, out_size - used - 1, f);
        if (n == 0) break;
        used += n;
    }
    out[used] = '\0';
    fclose(f);
    return 1;
}

static void show_answer_pages(const char *answer) {
    size_t len = strlen(answer);
    size_t pos = 0;
    int page = 1;
    char pagebuf[PAGE_SIZE + 64];

    if (len == 0) {
        show_msgbox("NspireAI", "(empty response)");
        return;
    }

    while (pos < len && page <= 4) {
        size_t take = len - pos;
        if (take > PAGE_SIZE) take = PAGE_SIZE;

        /* Try to break at a newline/space near the end of the page. */
        if (pos + take < len) {
            size_t j = take;
            while (j > PAGE_SIZE / 2 &&
                   answer[pos + j] != '\n' &&
                   answer[pos + j] != ' ') {
                --j;
            }
            if (j > PAGE_SIZE / 2) take = j;
        }

        snprintf(pagebuf, sizeof(pagebuf), "Page %d\n\n%.*s",
                 page, (int)take, answer + pos);
        show_msgbox("NspireAI - Gemini", pagebuf);

        pos += take;
        while (answer[pos] == ' ' || answer[pos] == '\n' || answer[pos] == '\r')
            ++pos;
        ++page;
    }

    if (pos < len) {
        show_msgbox("NspireAI", "Response was longer than the display limit.");
    }
}

int main(int argc, char **argv) {
    char *prompt = NULL;
    char response[RESPONSE_MAX];
    unsigned long request_id;
    int result;

    assert_ndless_rev(0);
    enable_relative_paths(argv);

    show_msgbox("NspireAI",
        "Gemini frontend v0.4\n\n"
        "The calculator writes request.tns.\n"
        "Your Windows bridge creates response.tns.");

    if (!show_msg_user_input("NspireAI", "Ask Gemini:", "", &prompt) ||
        prompt == NULL || prompt[0] == '\0') {
        if (prompt) free(prompt);
        return 0;
    }

    if (strlen(prompt) >= PROMPT_MAX) {
        show_msgbox("NspireAI", "Prompt is too long.");
        free(prompt);
        return 0;
    }

    request_id = make_request_id(prompt);

    if (!write_request(request_id, prompt)) {
        show_msgbox("NspireAI", "Could not create request.tns.");
        free(prompt);
        return 0;
    }

    free(prompt);

    show_msgbox("Request created",
        "Now:\n"
        "1. Copy request.tns to host/exchange on PC.\n"
        "2. Run RUN_ONCE.bat.\n"
        "3. Copy response.tns back to this calculator folder.\n"
        "4. Press OK here.");

    memset(response, 0, sizeof(response));
    result = read_response(request_id, response, sizeof(response));

    if (result == 1) {
        show_answer_pages(response);
    } else if (result == -1) {
        show_msgbox("NspireAI",
            "response.tns is for an older request.\n"
            "Transfer the new response and reopen NspireAI.");
    } else {
        show_msgbox("NspireAI",
            "No valid response.tns found.\n"
            "Transfer it to the same folder as NspireAI, then reopen the app.");
    }

    return 0;
}
