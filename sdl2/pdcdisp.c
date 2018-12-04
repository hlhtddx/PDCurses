/* Public Domain Curses */

#include "pdcsdl.h"

#include <stdlib.h>
#include <string.h>

chtype acs_map[128] =
{
    PDC_ACS(0), PDC_ACS(1), PDC_ACS(2), PDC_ACS(3), PDC_ACS(4),
    PDC_ACS(5), PDC_ACS(6), PDC_ACS(7), PDC_ACS(8), PDC_ACS(9),
    PDC_ACS(10), PDC_ACS(11), PDC_ACS(12), PDC_ACS(13), PDC_ACS(14),
    PDC_ACS(15), PDC_ACS(16), PDC_ACS(17), PDC_ACS(18), PDC_ACS(19),
    PDC_ACS(20), PDC_ACS(21), PDC_ACS(22), PDC_ACS(23), PDC_ACS(24),
    PDC_ACS(25), PDC_ACS(26), PDC_ACS(27), PDC_ACS(28), PDC_ACS(29),
    PDC_ACS(30), PDC_ACS(31), ' ', '!', '"', '#', '$', '%', '&', '\'',
    '(', ')', '*',

#ifdef PDC_WIDE
    0x2192, 0x2190, 0x2191, 0x2193,
#else
    PDC_ACS(0x1a), PDC_ACS(0x1b), PDC_ACS(0x18), PDC_ACS(0x19),
#endif

    '/',

#ifdef PDC_WIDE
    ACS_BLOCK,
#else
    0xdb,
#endif

    '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=',
    '>', '?', '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z', '[', '\\', ']', '^', '_',

#ifdef PDC_WIDE
    0x2666, 0x2592,
#else
    PDC_ACS(0x04), 0xb1,
#endif

    'b', 'c', 'd', 'e',

#ifdef PDC_WIDE
    0x00b0, 0x00b1, 0x2591, 0x00a4, ACS_LRCORNER, ACS_URCORNER,
    ACS_ULCORNER, ACS_LLCORNER, ACS_PLUS, ACS_S1, ACS_S3, ACS_HLINE,
    ACS_S7, ACS_S9, ACS_LTEE, ACS_RTEE, ACS_BTEE, ACS_TTEE, ACS_VLINE,
    0x2264, 0x2265, 0x03c0, 0x2260, 0x00a3, 0x00b7,
#else
    0xf8, 0xf1, 0xb0, PDC_ACS(0x0f), 0xd9, 0xbf, 0xda, 0xc0, 0xc5, 0x2d,
    0x2d, 0xc4, 0x2d, 0x5f, 0xc3, 0xb4, 0xc1, 0xc2, 0xb3, 0xf3, 0xf2,
    0xe3, 0xd8, 0x9c, 0xf9,
#endif

    PDC_ACS(127)
};

Uint32 pdc_lastupdate = 0;

#define MAXRECT 200     /* maximum number of rects to queue up before
                           an update is forced; the number was chosen
                           arbitrarily */

static SDL_Rect uprect[MAXRECT];       /* table of rects to update */
static chtype oldch = (chtype)(-1);    /* current attribute */
static int rectcount = 0;              /* index into uprect */
static short foregr = -2, backgr = -2; /* current foreground, background */
static bool blinked_off = FALSE;

/* do the real updates on a delay */

void PDC_update_rects(void)
{
    if (rectcount)
    {
        /* if the maximum number of rects has been reached, we're
           probably better off doing a full screen update */

        if (rectcount == MAXRECT)
            SDL_UpdateWindowSurface(pdc_window);
        else
            SDL_UpdateWindowSurfaceRects(pdc_window, uprect, rectcount);

        pdc_lastupdate = SDL_GetTicks();
        rectcount = 0;
    }
}

/* set the font colors to match the chtype's attribute */

static void _set_attr(chtype ch)
{
    attr_t sysattrs = SP->termattrs;

#ifdef PDC_WIDE
    TTF_SetFontStyle(pdc_ttffont,
        ( ((ch & A_BOLD) && (sysattrs & A_BOLD)) ?
            TTF_STYLE_BOLD : 0) |
        ( ((ch & A_ITALIC) && (sysattrs & A_ITALIC)) ?
            TTF_STYLE_ITALIC : 0) );
#endif

    ch &= (A_COLOR|A_BOLD|A_BLINK|A_REVERSE);

    if (oldch != ch)
    {
        short newfg, newbg;

        if (SP->mono)
            return;

        PDC_pair_content(PAIR_NUMBER(ch), &newfg, &newbg);

        if ((ch & A_BOLD) && !(sysattrs & A_BOLD))
            newfg |= 8;
        if ((ch & A_BLINK) && !(sysattrs & A_BLINK))
            newbg |= 8;

        if (ch & A_REVERSE)
        {
            short tmp = newfg;
            newfg = newbg;
            newbg = tmp;
        }

        if (newfg != foregr)
        {
#ifndef PDC_WIDE
            SDL_SetPaletteColors(pdc_font->format->palette,
                                 pdc_color + newfg, pdc_flastc, 1);
#endif
            foregr = newfg;
        }

        if (newbg != backgr)
        {
#ifndef PDC_WIDE
            if (newbg == -1)
                SDL_SetColorKey(pdc_font, SDL_TRUE, 0);
            else
            {
                if (backgr == -1)
                    SDL_SetColorKey(pdc_font, SDL_FALSE, 0);

                SDL_SetPaletteColors(pdc_font->format->palette,
                                     pdc_color + newbg, 0, 1);
            }
#endif
            backgr = newbg;
        }

        oldch = ch;
    }
}

#ifdef PDC_WIDE

/* Draw some of the ACS_* "graphics" */

bool _grprint(chtype ch, SDL_Rect dest)
{
    Uint32 col = pdc_mapped[foregr];
    int hmid = (pdc_fheight - pdc_fthick) >> 1;
    int wmid = (pdc_fwidth - pdc_fthick) >> 1;

    switch (ch)
    {
    case ACS_ULCORNER:
        dest.h = pdc_fheight - hmid;
        dest.y += hmid;
        dest.w = pdc_fthick;
        dest.x += wmid;
        SDL_FillRect(pdc_screen, &dest, col);
        dest.w = pdc_fwidth - wmid;
        goto S1;
    case ACS_LLCORNER:
        dest.h = hmid;
        dest.w = pdc_fthick;
        dest.x += wmid;
        SDL_FillRect(pdc_screen, &dest, col);
        dest.w = pdc_fwidth - wmid;
        dest.y += hmid;
        goto S1;
    case ACS_URCORNER:
        dest.h = pdc_fheight - hmid;
        dest.w = pdc_fthick;
        dest.y += hmid;
        dest.x += wmid;
        SDL_FillRect(pdc_screen, &dest, col);
        dest.w = wmid;
        dest.x -= wmid;
        goto S1;
    case ACS_LRCORNER:
        dest.h = hmid + pdc_fthick;
        dest.w = pdc_fthick;
        dest.x += wmid;
        SDL_FillRect(pdc_screen, &dest, col);
        dest.w = wmid;
        dest.x -= wmid;
        dest.y += hmid;
        goto S1;
    case ACS_LTEE:
        dest.h = pdc_fthick;
        dest.w = pdc_fwidth - wmid;
        dest.x += wmid;
        dest.y += hmid;
        SDL_FillRect(pdc_screen, &dest, col);
        dest.w = pdc_fthick;
        dest.x -= wmid;
        goto VLINE;
    case ACS_RTEE:
        dest.w = wmid;
    case ACS_PLUS:
        dest.h = pdc_fthick;
        dest.y += hmid;
        SDL_FillRect(pdc_screen, &dest, col);
    VLINE:
        dest.h = pdc_fheight;
        dest.y -= hmid;
    case ACS_VLINE:
        dest.w = pdc_fthick;
        dest.x += wmid;
        goto DRAW;
    case ACS_TTEE:
        dest.h = pdc_fheight - hmid;
        dest.w = pdc_fthick;
        dest.x += wmid;
        dest.y += hmid;
        SDL_FillRect(pdc_screen, &dest, col);
        dest.w = pdc_fwidth;
        dest.x -= wmid;
        goto S1;
    case ACS_BTEE:
        dest.h = hmid;
        dest.w = pdc_fthick;
        dest.x += wmid;
        SDL_FillRect(pdc_screen, &dest, col);
        dest.w = pdc_fwidth;
        dest.x -= wmid;
    case ACS_HLINE:
        dest.y += hmid;
        goto S1;
    case ACS_S3:
        dest.y += hmid >> 1;
        goto S1;
    case ACS_S7:
        dest.y += hmid + (hmid >> 1);
        goto S1;
    case ACS_S9:
        dest.y += pdc_fheight - pdc_fthick;
    case ACS_S1:
    S1:
        dest.h = pdc_fthick;
    case ACS_BLOCK:
    DRAW:
        SDL_FillRect(pdc_screen, &dest, col);
        return TRUE;
    default: ;
    }

    return FALSE;  /* didn't draw it -- fall back to acs_map */
}

#endif

/* draw a cursor at (y, x) */

void PDC_gotoyx(int row, int col)
{
    SDL_Rect src, dest;
    chtype ch;
    int oldrow, oldcol;
#ifdef PDC_WIDE
    Uint16 chstr[2] = {0, 0};
#endif

    PDC_LOG(("PDC_gotoyx() - called: row %d col %d from row %d col %d\n",
             row, col, SP->cursrow, SP->curscol));

    oldrow = SP->cursrow;
    oldcol = SP->curscol;

    /* clear the old cursor */

    PDC_transform_line(oldrow, oldcol, 1, curscr->_y[oldrow] + oldcol);

    if (!SP->visibility)
        return;

    /* draw a new cursor by overprinting the existing character in
       reverse, either the full cell (when visibility == 2) or the
       lowest quarter of it (when visibility == 1) */

    ch = curscr->_y[row][col] ^ A_REVERSE;

    _set_attr(ch);

    src.h = (SP->visibility == 1) ? pdc_fheight >> 2 : pdc_fheight;
    src.w = pdc_fwidth;

    dest.y = (row + 1) * pdc_fheight - src.h + pdc_yoffset;
    dest.x = col * pdc_fwidth + pdc_xoffset;
    dest.h = src.h;
    dest.w = src.w;

#ifdef PDC_WIDE
    SDL_FillRect(pdc_screen, &dest, pdc_mapped[backgr]);

    if (!(SP->visibility == 2 && (ch & A_ALTCHARSET && !(ch & 0xff80)) &&
        _grprint(ch & (0x7f | A_ALTCHARSET), dest)))
    {
        if (ch & A_ALTCHARSET && !(ch & 0xff80))
            ch = acs_map[ch & 0x7f];

        chstr[0] = ch & A_CHARTEXT;

        pdc_font = TTF_RenderUNICODE_Blended(pdc_ttffont, chstr,
                                             pdc_color[foregr]);
        if (pdc_font)
        {
            int center = pdc_fwidth > pdc_font->w ?
                        (pdc_fwidth - pdc_font->w) >> 1 : 0;
            src.x = 0;
            src.y = pdc_fheight - src.h;
            dest.x += center;
            SDL_BlitSurface(pdc_font, &src, pdc_screen, &dest);
            dest.x -= center;
            SDL_FreeSurface(pdc_font);
            pdc_font = NULL;
        }
    }
#else
    if (ch & A_ALTCHARSET && !(ch & 0xff80))
        ch = acs_map[ch & 0x7f];

    src.x = (ch & 0xff) % 32 * pdc_fwidth;
    src.y = (ch & 0xff) / 32 * pdc_fheight + (pdc_fheight - src.h);

    SDL_BlitSurface(pdc_font, &src, pdc_screen, &dest);
#endif

    if (oldrow != row || oldcol != col)
    {
        if (rectcount == MAXRECT)
            PDC_update_rects();

        uprect[rectcount++] = dest;
    }
}

void _new_packet(attr_t attr, int lineno, int x, int len, const chtype *srcp)
{
    SDL_Rect src, dest, lastrect;
    int j;
#ifdef PDC_WIDE
    Uint16 chstr[2] = {0, 0};
#endif
    attr_t sysattrs = SP->termattrs;
    short hcol = SP->line_color;
    bool blink = blinked_off && (attr & A_BLINK) && (sysattrs & A_BLINK);

    if (rectcount == MAXRECT)
        PDC_update_rects();

#ifdef PDC_WIDE
    src.x = 0;
    src.y = 0;
#endif
    src.h = pdc_fheight;
    src.w = pdc_fwidth;

    dest.y = pdc_fheight * lineno + pdc_yoffset;
    dest.x = pdc_fwidth * x + pdc_xoffset;
    dest.h = pdc_fheight;
    dest.w = pdc_fwidth * len;

    /* if the previous rect was just above this one, with the same width
       and horizontal position, then merge the new one with it instead
       of adding a new entry */

    if (rectcount)
        lastrect = uprect[rectcount - 1];

    if (rectcount && lastrect.x == dest.x && lastrect.w == dest.w)
    {
        if (lastrect.y + lastrect.h == dest.y)
            uprect[rectcount - 1].h = lastrect.h + pdc_fheight;
        else
            if (lastrect.y != dest.y)
                uprect[rectcount++] = dest;
    }
    else
        uprect[rectcount++] = dest;

    _set_attr(attr);

    if (backgr == -1)
        SDL_LowerBlit(pdc_tileback, &dest, pdc_screen, &dest);
#ifdef PDC_WIDE
    else
        SDL_FillRect(pdc_screen, &dest, pdc_mapped[backgr]);
#endif

    if (hcol == -1)
        hcol = foregr;

    for (j = 0; j < len; j++)
    {
        chtype ch = srcp[j];

        if (blink)
            ch = ' ';

        dest.w = pdc_fwidth;

        if (ch & A_ALTCHARSET && !(ch & 0xff80))
        {
#ifdef PDC_WIDE
            if (_grprint(ch & (0x7f | A_ALTCHARSET), dest))
            {
                dest.x += pdc_fwidth;
                continue;
            }
#endif
            ch = acs_map[ch & 0x7f];
        }

#ifdef PDC_WIDE
        ch &= A_CHARTEXT;

        if (ch != ' ')
        {
            if (chstr[0] != ch)
            {
                chstr[0] = ch;

                if (pdc_font)
                    SDL_FreeSurface(pdc_font);

                pdc_font = TTF_RenderUNICODE_Blended(pdc_ttffont, chstr,
                                                     pdc_color[foregr]);
            }

            if (pdc_font)
            {
                int center = pdc_fwidth > pdc_font->w ?
                    (pdc_fwidth - pdc_font->w) >> 1 : 0;
                dest.x += center;
                SDL_BlitSurface(pdc_font, &src, pdc_screen, &dest);
                dest.x -= center;
            }
        }
#else
        src.x = (ch & 0xff) % 32 * pdc_fwidth;
        src.y = (ch & 0xff) / 32 * pdc_fheight;

        SDL_LowerBlit(pdc_font, &src, pdc_screen, &dest);
#endif

        if (!blink && (attr & (A_LEFT | A_RIGHT)))
        {
            dest.w = pdc_fthick;

            if (attr & A_LEFT)
                SDL_FillRect(pdc_screen, &dest, pdc_mapped[hcol]);

            if (attr & A_RIGHT)
            {
                dest.x += pdc_fwidth - pdc_fthick;
                SDL_FillRect(pdc_screen, &dest, pdc_mapped[hcol]);
                dest.x -= pdc_fwidth - pdc_fthick;
            }
        }

        dest.x += pdc_fwidth;
    }

#ifdef PDC_WIDE
    if (pdc_font)
    {
        SDL_FreeSurface(pdc_font);
        pdc_font = NULL;
    }
#endif

    if (!blink && (attr & A_UNDERLINE))
    {
        dest.y += pdc_fheight - pdc_fthick;
        dest.x = pdc_fwidth * x + pdc_xoffset;
        dest.h = pdc_fthick;
        dest.w = pdc_fwidth * len;

        SDL_FillRect(pdc_screen, &dest, pdc_mapped[hcol]);
    }
}

/* update the given physical line to look like the corresponding line in
   curscr */

void PDC_transform_line(int lineno, int x, int len, const chtype *srcp)
{
    attr_t old_attr, attr;
    int i, j;

    PDC_LOG(("PDC_transform_line() - called: lineno=%d\n", lineno));

    old_attr = *srcp & (A_ATTRIBUTES ^ A_ALTCHARSET);

    for (i = 1, j = 1; j < len; i++, j++)
    {
        attr = srcp[i] & (A_ATTRIBUTES ^ A_ALTCHARSET);

        if (attr != old_attr)
        {
            _new_packet(old_attr, lineno, x, i, srcp);
            old_attr = attr;
            srcp += i;
            x += i;
            i = 0;
        }
    }

    _new_packet(old_attr, lineno, x, i, srcp);
}

static Uint32 _blink_timer(Uint32 interval, void *param)
{
    SDL_Event event;

    event.type = SDL_USEREVENT;
    SDL_PushEvent(&event);
    return(interval);
}

void PDC_blink_text(void)
{
    static SDL_TimerID blinker_id = 0;
    int i, j, k;

    oldch = (chtype)(-1);

    if (!(SP->termattrs & A_BLINK))
    {
        SDL_RemoveTimer(blinker_id);
        blinker_id = 0;
    }
    else if (!blinker_id)
    {
        blinker_id = SDL_AddTimer(500, _blink_timer, NULL);
        blinked_off = TRUE;
    }

    blinked_off = !blinked_off;

    for (i = 0; i < SP->lines; i++)
    {
        const chtype *srcp = curscr->_y[i];

        for (j = 0; j < SP->cols; j++)
            if (srcp[j] & A_BLINK)
            {
                k = j;
                while (k < SP->cols && (srcp[k] & A_BLINK))
                    k++;
                PDC_transform_line(i, j, k - j, srcp + j);
                j = k;
            }
    }

    oldch = (chtype)(-1);
}
