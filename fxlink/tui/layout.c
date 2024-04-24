//---------------------------------------------------------------------------//
// ==>/[_]\   fxlink: A community communication tool for CASIO calculators.  //
//    |:::|   Made by Lephe' as part of the fxSDK.                           //
//    \___/   License: MIT <https://opensource.org/licenses/MIT>             //
//---------------------------------------------------------------------------//

#include <fxlink/tui/layout.h>
#include <stdlib.h>
#include <math.h>

typedef unsigned int uint;

//---
// Flexbox-like allocation algorithm (copy/pasted from JustUI)
//
// The basic idea of space redistribution is to give each widget extra space
// proportional to their stretch rates in the relevant direction. However, the
// addition of maximum size constraints means that widgets can decline some of
// the extra space being allocated.
//
// This system defines the result of expansion as a function of the "expansion
// factor". As the expansion factor increases, every widget stretches at a
// speed proportional to its stretch rate, until it reaches its maximum size.
//
//  Extra widget size
//   |
//   +      .-------- Maximum size
//   |    .`
//   |  .` <- Slope: widget stretch rate
//   |.`
// 0 +-------+------> Expansion factor
//   0       ^
//      Breaking point
//
// The extra space allocated to widgets is the sum of this function for every
// widget considered for expansion. Since every widget has a possibly different
// breaking point, a maximal interval of expansion factor that has no breaking
// point is called a "run". During each run, the slope for the total space
// remains constant, and a unit of expansion factor corresponds to one pixel
// being allocated in the container. Thus, whenever the expansion factor
// increases of (slope), every widget (w) gets (w->stretch) new pixels.
//
// The functions below simulate the expansion by determining the breaking
// points of the widgets and allocating extra space during each run. Once the
// total extra space allocated reaches the available space, simulation stops
// and the allocation is recorded by assigning actual size to widgets.
//---

/* This "expansion" structure tracks information relating to a single child
   widget during the space distribution process. */
typedef struct {
    /* Child index */
    uint8_t id;
    /* Stretch rate, sum of stretch rates is the "slope" */
    uint8_t stretch;
    /* Maximum size augmentation */
    int16_t max;
    /* Extra space allocate in the previous runs, in pixels */
    float allocated;
    /* Breaking point for the current run, as a number of pixels to distribute
       to the whole system */
    float breaking_point;
} exp_t;

/* Determine whether a widget can expand any further. */
static bool can_expand(exp_t *e)
{
    return (e->stretch > 0 && e->allocated < e->max);
}

/* Compute the slope for the current run. */
static uint compute_slope(exp_t elements[], size_t n)
{
    uint slope = 0;
    for(size_t i = 0; i < n; i++) {
        if(can_expand(&elements[i])) slope += elements[i].stretch;
    }
    return slope;
}

/* Compute the breaking point for every expanding widget. Returns the amount of
   pixels to allocate in order to reach the next breaking point. */
static float compute_breaking_points(exp_t elements[], size_t n, uint slope)
{
    float closest = HUGE_VALF;

    for(size_t i = 0; i < n; i++) {
        exp_t *e = &elements[i];
        if(!can_expand(e)) continue;

        /* Up to (e->max - e->allocated) pixels can be added to this widget.
           With the factor of (slope / e->stretch), we get the number of pixels
           to add to the container in order to reach the threshold. */
        e->breaking_point = (e->max - e->allocated) * (slope / e->stretch);
        closest = fminf(e->breaking_point, closest);
    }

    return closest;
}

/* Allocate floating-point space to widgets. This is the core of the
   distribution system, it produces (e->allocated) for every element. */
static void allocate_space(exp_t elements[], size_t n, float available)
{
    /* One iteration per run */
    while(available > 0) {
        /* Slope for this run; if zero, no more widget can grow */
        uint slope = compute_slope(elements, n);
        if(!slope) break;

        /* Closest breaking point, amount of space to distribute this run */
        float breaking = compute_breaking_points(elements, n, slope);
        float run_budget = fminf(breaking, available);

        /* Give everyone their share of run_budget */
        for(size_t i = 0; i < n; i++) {
            exp_t *e = &elements[i];
            if(!can_expand(e)) continue;

            e->allocated += (run_budget * e->stretch) / slope;
        }

        available -= run_budget;
    }
}

/* Stable insertion sort: order children by decreasing fractional allocation */
static void sort_by_fractional_allocation(exp_t elements[], size_t n)
{
    for(size_t spot = 0; spot < n - 1; spot++) {
        /* Find the element with the max fractional value in [spot..size] */
        float max_frac = 0;
        int max_frac_who = -1;

        for(size_t i = spot; i < n; i++) {
            exp_t *e = &elements[i];

            float frac = e->allocated - floorf(e->allocated);

            if(max_frac_who < 0 || frac > max_frac) {
                max_frac = frac;
                max_frac_who = i;
            }
        }

        /* Give that element the spot */
        exp_t temp = elements[spot];
        elements[spot] = elements[max_frac_who];
        elements[max_frac_who] = temp;
    }
}

static int compare_ids(void const *ptr1, void const *ptr2)
{
	exp_t const *e1 = ptr1;
	exp_t const *e2 = ptr2;
	return e1->id - e2->id;
}

/* Round allocations so that they add up to the available space */
static void round_allocations(exp_t elements[], size_t n, int available_space)
{
    /* Prepare to give everyone the floor of their allocation */
    for(size_t i = 0; i < n; i++) {
        exp_t *e = &elements[i];
        available_space -= floorf(e->allocated);
    }

    /* Sort by decreasing fractional allocation then add one extra pixel to
       the (available_space) children with highest fractional allocation */
    sort_by_fractional_allocation(elements, n);

    for(size_t i = 0; i < n; i++) {
        exp_t *e = &elements[i];
        e->allocated = floorf(e->allocated);

        if(can_expand(e) && (int)i < available_space) e->allocated += 1;
    }

    /* Sort back by IDs for final ordering */
	qsort(elements, n, sizeof *elements, compare_ids);
}

//---
// TUI layout
//---

static struct fxlink_TUI_box *mkbox(void)
{
	struct fxlink_TUI_box *b = calloc(1, sizeof *b);
	if(b) {
		b->max_w = 0xffff;
		b->max_h = 0xffff;
		b->stretch_x = 1;
		b->stretch_y = 1;
	}
	return b;
}

struct fxlink_TUI_box *fxlink_TUI_box_mk_window(char const *title, WINDOW **w)
{
	struct fxlink_TUI_box *b = mkbox();
	if(b) {
		b->type = FXLINK_TUI_BOX_WINDOW;
		b->window.title = title;
		b->window.win = w;
	}
	return b;
}

static struct fxlink_TUI_box *mkcontainer(int type,
	struct fxlink_TUI_box *child, va_list args)
{
	struct fxlink_TUI_box *b = mkbox();
	if(b) {
		b->type = type;
		int i = 0;

		while(child && i < FXLINK_TUI_BOX_MAXSIZE) {
			b->children[i++] = child;
			child = va_arg(args, struct fxlink_TUI_box *);
		}
	}
	return b;
}

struct fxlink_TUI_box *fxlink_TUI_box_mk_vertical(
	struct fxlink_TUI_box *child1, ...)
{
	va_list args;
	va_start(args, child1);
	return mkcontainer(FXLINK_TUI_BOX_VERTICAL, child1, args);
	va_end(args);
}

struct fxlink_TUI_box *fxlink_TUI_box_mk_horizontal(
	struct fxlink_TUI_box *child1, ...)
{
	va_list args;
	va_start(args, child1);
	return mkcontainer(FXLINK_TUI_BOX_HORIZONTAL, child1, args);
	va_end(args);
}

void fxlink_TUI_box_minsize(struct fxlink_TUI_box *box, int min_w, int min_h)
{
	box->min_w = min_w;
	box->min_h = min_h;
}

void fxlink_TUI_box_maxsize(struct fxlink_TUI_box *box, int max_w, int max_h)
{
	box->max_w = max_w;
	box->max_h = max_h;
}

void fxlink_TUI_box_stretch(struct fxlink_TUI_box *box, int stretch_x,
	int stretch_y, bool force)
{
	box->stretch_x = stretch_x;
	box->stretch_y = stretch_y;
	box->stretch_force = force;
}

void fxlink_TUI_box_print(FILE *fp, struct fxlink_TUI_box const *b, int level)
{
	for(int i = 0; i < level * 4; i++)
		fputc(' ', fp);

	fprintf(fp, "type=");
	if(b->type == FXLINK_TUI_BOX_WINDOW)
		fprintf(fp, "WINDOW '%s'", b->window.title);
	if(b->type == FXLINK_TUI_BOX_VERTICAL)
		fprintf(fp, "VERTICAL");
	if(b->type == FXLINK_TUI_BOX_HORIZONTAL)
		fprintf(fp, "HORIZONTAL");

	fprintf(fp, " x=%d y=%d w=", b->x, b->y);

	if(b->min_w > 0)
		fprintf(fp, "(%d)<", b->min_w);
	fprintf(fp, "%d", b->w);
	if(b->max_w < 0xffff)
		fprintf(fp, "<(%d)", b->max_w);

	fprintf(fp, " h=");

	if(b->min_h > 0)
		fprintf(fp, "(%d)<", b->min_h);
	fprintf(fp, "%d", b->h);
	if(b->max_h < 0xffff)
		fprintf(fp, "<(%d)", b->max_h);

	fprintf(fp, "\n");

	if(b->type != FXLINK_TUI_BOX_WINDOW) {
		for(int i = 0; i < FXLINK_TUI_BOX_MAXSIZE && b->children[i]; i++)
			fxlink_TUI_box_print(fp, b->children[i], level+1);
	}
}

static void box_do_layout(struct fxlink_TUI_box *box)
{
	if(box->type == FXLINK_TUI_BOX_WINDOW)
		return;

	int horiz = (box->type == FXLINK_TUI_BOX_HORIZONTAL);
	size_t child_count = 0;
	while(child_count < FXLINK_TUI_BOX_MAXSIZE && box->children[child_count])
		child_count++;
	int spacing = 1;

	/* Content width and height */
	int cw = box->w;
	int ch = box->h;
	/* Allocatable width and height (which excludes spacing) */
	int total_spacing = (child_count - 1) * spacing;
	int aw = cw - (horiz ? total_spacing : 0);
	int ah = ch - (horiz ? 0 : total_spacing);
	/* Length along the main axis, including spacing */
	int length = 0;

	/* Expanding widgets' information for extra space distribution */
	size_t n = child_count;
	exp_t elements[n];

	for(size_t i = 0; i < child_count; i++) {
		struct fxlink_TUI_box *child = box->children[i];

		/* Maximum size to enforce: this is the acceptable size closest to our
		   allocatable size */
		int max_w = clamp(aw, child->min_w, child->max_w);
		int max_h = clamp(ah, child->min_h, child->max_h);

		/* Start by setting every child to an acceptable size */
		child->w = clamp(child->w, child->min_w, max_w);
		child->h = clamp(child->h, child->min_h, max_h);

		/* Initialize expanding widgets' information */
		elements[i].id = i;
		elements[i].allocated = 0.0f;
		elements[i].breaking_point = -1.0f;

		/* Determine natural length along the container, and stretch child
		   along the perpendicular direction if possible */

		if(i > 0)
			length += spacing;
		if(horiz) {
			length += child->w;
			if(child->stretch_y > 0) child->h = max_h;

			elements[i].stretch = child->stretch_x;
			elements[i].max = max(max_w - child->w, 0);

			if(child->stretch_force && child->stretch_x > 0)
				elements[i].max = max(aw - child->w, 0);
		}
		else {
			length += child->h;
			if(child->stretch_x > 0) child->w = max_w;

			elements[i].stretch = child->stretch_y;
			elements[i].max = max(max_h - child->h, 0);

			if(child->stretch_force && child->stretch_y > 0)
				elements[i].max = max(ah - child->h, 0);
		}
	}

	/* Distribute extra space along the line */
	int extra_space = (horiz ? cw : ch) - length;
	allocate_space(elements, n, extra_space);
	round_allocations(elements, n, extra_space);

	/* Update widgets for extra space */
	for(size_t i = 0; i < n; i++) {
		exp_t *e = &elements[i];
		struct fxlink_TUI_box *child = box->children[e->id];

		if(horiz)
			child->w += e->allocated;
		else
			child->h += e->allocated;
	}

	/* Position everyone */
	int position = 0;

	for(size_t i = 0; i < n; i++) {
		exp_t *e = &elements[i];
		struct fxlink_TUI_box *child = box->children[e->id];

		if(horiz) {
			child->x = box->x + position;
			child->y = box->y + (ch - child->h) / 2;
			position += child->w + spacing;
		}
		else {
			child->x = box->x + (cw - child->w) / 2;
			child->y = box->y + position;
			position += child->h + spacing;
		}
	}

	for(int i = 0; i < FXLINK_TUI_BOX_MAXSIZE && box->children[i]; i++)
		box_do_layout(box->children[i]);
}

void fxlink_TUI_box_layout(struct fxlink_TUI_box *b,
	int x, int y, int w, int h)
{
	b->x = x + 1;
	b->y = y + 1;
	b->w = w - 2;
	b->h = h - 2;
	return box_do_layout(b);
}

static bool box_apply(struct fxlink_TUI_box *box,
	int rx, int ry, int rw, int rh)
{
	if(box->type == FXLINK_TUI_BOX_WINDOW) {
		if(!box->window.win)
			return true;

		/* Ensure window is of non-zero size and in-bounds */
		int x = clamp(box->x, rx, rx + rw - 1);
		int y = clamp(box->y, ry, ry + rh - 1);
		int w = clamp(box->w, 1, rw - (x - rx));
		int h = clamp(box->h, 1, rh - (y - ry));

		if(!*box->window.win) {
			*box->window.win = newwin(h, w, y, x);
		}
		else {
			wresize(*box->window.win, h, w);
			mvwin(*box->window.win, y, x);
		}
		return (*box->window.win != NULL);
	}

	bool success = true;
	for(int i = 0; i < FXLINK_TUI_BOX_MAXSIZE && box->children[i]; i++)
		success = success && box_apply(box->children[i], rx, ry, rw, rh);
	return success;
}

bool fxlink_TUI_apply_layout(struct fxlink_TUI_box *root)
{
	return box_apply(root, root->x, root->y, root->w, root->h);
}
