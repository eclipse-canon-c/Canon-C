/*
 * diag.h: a chain of diagnostic frames pushed by three callers, read back
 * (depth, root, latest, each frame), rendered to a buffer, printed, then
 * cleared; and a push past the chain's capacity.
 */
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include "semantics/diag.h"
#include "eva_program.h"

static bool parse(Diag *d, int ok)
{
    if (!ok) { DIAG_PUSH(d, ERR_PARSE_FAILED, "bad digit"); }
    return ok != 0;
}

static bool load(Diag *d, int ok)
{
    if (!parse(d, ok)) { DIAG_PUSH(d, ERR_INVALID_FORMAT, "config line"); return false; }
    return true;
}

int main(void)
{
    int fails = 0;
    Diag d = diag_init();
    char out[512];

    CHECK(diag_depth(&d) == 0u && !diag_has_error(&d) && diag_root(&d) == NULL);
    CHECK(load(&d, 1) && diag_depth(&d) == 0u);
    CHECK(!load(&d, 0));
    CHECK(!diag_push(&d, "diag.c", 40u, "main", ERR_IO_FAILED, "startup")); /* nothing dropped */
    CHECK(diag_depth(&d) == 3u && diag_has_error(&d));
    CHECK(diag_root_code(&d) == ERR_PARSE_FAILED && diag_latest_code(&d) == ERR_IO_FAILED);
    const DiagFrame *root = diag_root(&d);
    const DiagFrame *latest = diag_latest(&d);
    CHECK(root != NULL && latest != NULL && diag_frame_at(&d, 1u) != NULL);
    CHECK(diag_frame_at(&d, 3u) == NULL);
    CHECK(root != NULL && root->message[0] == 'b');

    usize n = diag_render_frame(root, 0u, out, sizeof out);
    CHECK(n > 0u && n < sizeof out);
    n = diag_render(&d, out, sizeof out);
    CHECK(n > 0u && n < sizeof out);
    diag_print(&d, stdout);

    for (int i = 0; i < 8; i++) { (void)diag_push(&d, "diag.c", 50u, "main", ERR_UNKNOWN, "more"); }
    CHECK(diag_depth(&d) == DIAG_MAX_FRAMES);
    diag_clear(&d);
    CHECK(diag_depth(&d) == 0u && diag_root_code(&d) == ERR_OK);

    END(fails);
    return fails != 0;
}
