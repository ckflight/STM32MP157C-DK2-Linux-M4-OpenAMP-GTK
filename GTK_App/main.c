#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <glob.h>
#include <termios.h>

#define N                  220
#define PACKET_HEADER      0xA55A

#define ACC_SCALE          2048.0      // ±16 g
#define GYRO_SCALE         16.4        // ±2000 deg/s


/*
 * STM32MP157 GTK IMU Monitor
 *
 * Displays real-time IMU data received from the Cortex-M4
 * through OpenAMP/RPMsg.
 *
 * Build & deploy:
 *   chmod +x build.sh
 *   ./build.sh
 *
 * build.sh cross-compiles the application and copies the executable
 * to STM32MP157 Linux over Ethernet using SCP.
 *
 * Make sure the target IP address in build.sh is correct.
 */

typedef struct __attribute__((packed))
{
    uint16_t header;

    int16_t ax;
    int16_t ay;
    int16_t az;

    int16_t gx;
    int16_t gy;
    int16_t gz;

} SensorPacket;


/* =========================================================
 * GTK widgets
 * ========================================================= */

static GtkWidget *drawing_area;

static GtkWidget *status_label;
static GtkWidget *rate_label;
static GtkWidget *packet_label;
static GtkWidget *device_label;

static GtkWidget *x_value_label;
static GtkWidget *y_value_label;
static GtkWidget *z_value_label;

static GtkWidget *graph_title;

static GtkWidget *accel_button;
static GtkWidget *gyro_button;


/* =========================================================
 * RPMsg
 * ========================================================= */

static int rpmsg_fd = -1;

static char rpmsg_device[128] = "Not connected";

static uint8_t rx_stream[1024];
static size_t rx_stream_len = 0;


/* =========================================================
 * Statistics
 * ========================================================= */

static uint32_t received_this_second = 0;
static uint64_t total_packets = 0;


/* =========================================================
 * Display mode
 *
 * 0 = Accelerometer
 * 1 = Gyroscope
 * ========================================================= */

static int display_mode = 0;


/* =========================================================
 * Samples
 * ========================================================= */

static double acc_x[N];
static double acc_y[N];
static double acc_z[N];

static double gyro_x[N];
static double gyro_y[N];
static double gyro_z[N];

static int sample_count = 0;


/* =========================================================
 * Last values
 * ========================================================= */

static double last_ax = 0.0;
static double last_ay = 0.0;
static double last_az = 0.0;

static double last_gx = 0.0;
static double last_gy = 0.0;
static double last_gz = 0.0;


/* =========================================================
 * RPMsg auto detect
 * ========================================================= */

static int open_rpmsg_auto(void)
{
    glob_t paths;
    memset(&paths, 0, sizeof(paths));

    if (glob("/dev/ttyRPMSG*", 0, NULL, &paths) != 0 ||
        paths.gl_pathc == 0)
    {
        fprintf(stderr, "No ttyRPMSG device found\n");
        globfree(&paths);
        return -1;
    }

    for (size_t i = 0; i < paths.gl_pathc; i++)
    {
        int fd = open(paths.gl_pathv[i],
                      O_RDWR | O_NOCTTY | O_NONBLOCK);

        if (fd < 0)
            continue;

        struct termios tio;

        if (tcgetattr(fd, &tio) == 0)
        {
            cfmakeraw(&tio);

            tio.c_cc[VMIN]  = 0;
            tio.c_cc[VTIME] = 0;

            if (tcsetattr(fd, TCSANOW, &tio) != 0)
                perror("tcsetattr");

            tcflush(fd, TCIOFLUSH);
        }

        snprintf(rpmsg_device,
                 sizeof(rpmsg_device),
                 "%s",
                 paths.gl_pathv[i]);

        printf("RPMsg selected: %s\n",
               rpmsg_device);

        /*
         * Handshake:
         * lets M4 learn Linux endpoint address.
         */
        uint8_t hello = 0x55;
        write(fd, &hello, 1);

        globfree(&paths);

        return fd;
    }

    globfree(&paths);

    return -1;
}


/* =========================================================
 * Add one IMU sample
 * ========================================================= */

static void add_sample(
    double ax,
    double ay,
    double az,
    double gx,
    double gy,
    double gz)
{
    if (sample_count < N)
    {
        acc_x[sample_count] = ax;
        acc_y[sample_count] = ay;
        acc_z[sample_count] = az;

        gyro_x[sample_count] = gx;
        gyro_y[sample_count] = gy;
        gyro_z[sample_count] = gz;

        sample_count++;

        return;
    }

    memmove(acc_x, acc_x + 1, sizeof(double) * (N - 1));
    memmove(acc_y, acc_y + 1, sizeof(double) * (N - 1));
    memmove(acc_z, acc_z + 1, sizeof(double) * (N - 1));

    memmove(gyro_x, gyro_x + 1, sizeof(double) * (N - 1));
    memmove(gyro_y, gyro_y + 1, sizeof(double) * (N - 1));
    memmove(gyro_z, gyro_z + 1, sizeof(double) * (N - 1));

    acc_x[N - 1] = ax;
    acc_y[N - 1] = ay;
    acc_z[N - 1] = az;

    gyro_x[N - 1] = gx;
    gyro_y[N - 1] = gy;
    gyro_z[N - 1] = gz;
}


/* =========================================================
 * Update XYZ value cards
 * ========================================================= */

static void update_value_labels(void)
{
    char txt[64];

    if (display_mode == 0)
    {
        snprintf(txt, sizeof(txt), "%.3f g", last_ax);
        gtk_label_set_text(GTK_LABEL(x_value_label), txt);

        snprintf(txt, sizeof(txt), "%.3f g", last_ay);
        gtk_label_set_text(GTK_LABEL(y_value_label), txt);

        snprintf(txt, sizeof(txt), "%.3f g", last_az);
        gtk_label_set_text(GTK_LABEL(z_value_label), txt);
    }
    else
    {
        snprintf(txt, sizeof(txt), "%.1f deg/s", last_gx);
        gtk_label_set_text(GTK_LABEL(x_value_label), txt);

        snprintf(txt, sizeof(txt), "%.1f deg/s", last_gy);
        gtk_label_set_text(GTK_LABEL(y_value_label), txt);

        snprintf(txt, sizeof(txt), "%.1f deg/s", last_gz);
        gtk_label_set_text(GTK_LABEL(z_value_label), txt);
    }
}


/* =========================================================
 * Process valid packet
 * ========================================================= */

static void process_packet(const SensorPacket *p)
{
    last_ax = p->ax / ACC_SCALE;
    last_ay = p->ay / ACC_SCALE;
    last_az = p->az / ACC_SCALE;

    last_gx = p->gx / GYRO_SCALE;
    last_gy = p->gy / GYRO_SCALE;
    last_gz = p->gz / GYRO_SCALE;

    add_sample(
        last_ax,
        last_ay,
        last_az,
        last_gx,
        last_gy,
        last_gz
    );

    received_this_second++;
    total_packets++;

    update_value_labels();

    gtk_label_set_text(
        GTK_LABEL(status_label),
        "● M4 ONLINE");

    char txt[64];

    snprintf(
        txt,
        sizeof(txt),
        "%llu packets",
        (unsigned long long)total_packets
    );

    gtk_label_set_text(
        GTK_LABEL(packet_label),
        txt);

    gtk_widget_queue_draw(
        drawing_area);
}


/* =========================================================
 * RPMsg stream parser
 *
 * Finds:
 *
 * 5A A5
 *
 * which is little-endian 0xA55A.
 * ========================================================= */

static gboolean update_data(gpointer data)
{
    (void)data;

    if (rpmsg_fd < 0)
        return G_SOURCE_CONTINUE;

    uint8_t temp[256];

    ssize_t n;

    while ((n = read(
                rpmsg_fd,
                temp,
                sizeof(temp))) > 0)
    {
        /*
         * Avoid stream buffer overflow.
         */
        if (rx_stream_len + (size_t)n > sizeof(rx_stream))
        {
            rx_stream_len = 0;
        }

        memcpy(
            rx_stream + rx_stream_len,
            temp,
            n);

        rx_stream_len += n;

        /*
         * Search complete packets.
         */
        while (rx_stream_len >= sizeof(SensorPacket))
        {
            /*
             * 0xA55A in little endian:
             *
             * 5A A5
             */
            if (rx_stream[0] != 0x5A ||
                rx_stream[1] != 0xA5)
            {
                memmove(
                    rx_stream,
                    rx_stream + 1,
                    rx_stream_len - 1);

                rx_stream_len--;

                continue;
            }

            SensorPacket packet;

            memcpy(
                &packet,
                rx_stream,
                sizeof(packet));

            /*
             * Additional header validation.
             */
            if (packet.header != PACKET_HEADER)
            {
                memmove(
                    rx_stream,
                    rx_stream + 1,
                    rx_stream_len - 1);

                rx_stream_len--;

                continue;
            }

            process_packet(&packet);

            /*
             * Remove complete packet.
             */
            size_t remaining =
                rx_stream_len - sizeof(SensorPacket);

            memmove(
                rx_stream,
                rx_stream + sizeof(SensorPacket),
                remaining);

            rx_stream_len = remaining;
        }
    }

    if (n < 0 &&
        errno != EAGAIN &&
        errno != EWOULDBLOCK)
    {
        perror("RPMsg read");
    }

    return G_SOURCE_CONTINUE;
}


/* =========================================================
 * Packet rate
 * ========================================================= */

static gboolean update_rate(gpointer data)
{
    (void)data;

    char txt[64];

    snprintf(
        txt,
        sizeof(txt),
        "%u Hz",
        received_this_second);

    gtk_label_set_text(
        GTK_LABEL(rate_label),
        txt);

    if (received_this_second == 0)
    {
        gtk_label_set_text(
            GTK_LABEL(status_label),
            "● M4 OFFLINE");
    }

    received_this_second = 0;

    return G_SOURCE_CONTINUE;
}


/* =========================================================
 * Rounded rectangle
 * ========================================================= */

static void rounded_rect(
    cairo_t *cr,
    double x,
    double y,
    double w,
    double h,
    double r)
{
    cairo_new_sub_path(cr);

    cairo_arc(
        cr,
        x + w - r,
        y + r,
        r,
        -G_PI / 2,
        0);

    cairo_arc(
        cr,
        x + w - r,
        y + h - r,
        r,
        0,
        G_PI / 2);

    cairo_arc(
        cr,
        x + r,
        y + h - r,
        r,
        G_PI / 2,
        G_PI);

    cairo_arc(
        cr,
        x + r,
        y + r,
        r,
        G_PI,
        3 * G_PI / 2);

    cairo_close_path(cr);
}


/* =========================================================
 * Draw one axis
 * ========================================================= */

static void draw_axis(
    cairo_t *cr,
    double *samples,
    int count,
    double min_v,
    double max_v,
    double left,
    double top,
    double pw,
    double ph,
    double r,
    double g,
    double b)
{
    if (count < 2)
        return;

    double range = max_v - min_v;

    cairo_set_source_rgba(
        cr,
        r,
        g,
        b,
        0.12);

    cairo_set_line_width(cr, 7.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    for (int i = 1; i < count; i++)
    {
        double v1 = samples[i - 1];
        double v2 = samples[i];

        if (v1 < min_v) v1 = min_v;
        if (v1 > max_v) v1 = max_v;

        if (v2 < min_v) v2 = min_v;
        if (v2 > max_v) v2 = max_v;

        double x1 =
            left +
            ((double)(i - 1) / (N - 1)) * pw;

        double x2 =
            left +
            ((double)i / (N - 1)) * pw;

        double y1 =
            top +
            (1.0 - ((v1 - min_v) / range)) * ph;

        double y2 =
            top +
            (1.0 - ((v2 - min_v) / range)) * ph;

        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
    }

    cairo_stroke(cr);


    /*
     * Main signal
     */
    cairo_set_source_rgb(cr, r, g, b);

    cairo_set_line_width(cr, 2.2);

    for (int i = 1; i < count; i++)
    {
        double v1 = samples[i - 1];
        double v2 = samples[i];

        if (v1 < min_v) v1 = min_v;
        if (v1 > max_v) v1 = max_v;

        if (v2 < min_v) v2 = min_v;
        if (v2 > max_v) v2 = max_v;

        double x1 =
            left +
            ((double)(i - 1) / (N - 1)) * pw;

        double x2 =
            left +
            ((double)i / (N - 1)) * pw;

        double y1 =
            top +
            (1.0 - ((v1 - min_v) / range)) * ph;

        double y2 =
            top +
            (1.0 - ((v2 - min_v) / range)) * ph;

        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
    }

    cairo_stroke(cr);
}


/* =========================================================
 * Graph
 * ========================================================= */

static gboolean draw_graph(
    GtkWidget *widget,
    cairo_t *cr,
    gpointer data)
{
    (void)data;

    GtkAllocation a;

    gtk_widget_get_allocation(
        widget,
        &a);

    double w = a.width;
    double h = a.height;

    rounded_rect(
        cr,
        0,
        0,
        w,
        h,
        16);

    cairo_set_source_rgb(
        cr,
        0.043,
        0.059,
        0.082);

    cairo_fill(cr);


    double left   = 55;
    double right  = 20;
    double top    = 30;
    double bottom = 25;

    double pw = w - left - right;
    double ph = h - top - bottom;


    /*
     * Physical display range
     */
    double min_v;
    double max_v;

    if (display_mode == 0)
    {
        min_v = -16.0;
        max_v =  16.0;
    }
    else
    {
        min_v = -2000.0;
        max_v =  2000.0;
    }

    double range = max_v - min_v;


    /*
     * Grid
     */
    cairo_set_source_rgba(
        cr,
        1.0,
        1.0,
        1.0,
        0.055);

    cairo_set_line_width(cr, 1.0);

    for (int i = 0; i <= 4; i++)
    {
        double y =
            top +
            ((double)i / 4.0) * ph;

        cairo_move_to(cr, left, y);
        cairo_line_to(cr, left + pw, y);
    }

    for (int i = 0; i <= 8; i++)
    {
        double x =
            left +
            ((double)i / 8.0) * pw;

        cairo_move_to(cr, x, top);
        cairo_line_to(cr, x, top + ph);
    }

    cairo_stroke(cr);


    /*
     * Zero line
     */
    double zero_y =
        top +
        (max_v / range) * ph;

    cairo_set_source_rgba(
        cr,
        1.0,
        1.0,
        1.0,
        0.20);

    cairo_set_line_width(cr, 1.2);

    cairo_move_to(cr, left, zero_y);
    cairo_line_to(cr, left + pw, zero_y);

    cairo_stroke(cr);


    /*
     * Y-axis labels
     */
    cairo_select_font_face(
        cr,
        "Sans",
        CAIRO_FONT_SLANT_NORMAL,
        CAIRO_FONT_WEIGHT_NORMAL);

    cairo_set_font_size(cr, 10);

    cairo_set_source_rgba(
        cr,
        0.55,
        0.63,
        0.73,
        1.0);

    for (int i = 0; i <= 4; i++)
    {
        double frac =
            1.0 -
            ((double)i / 4.0);

        double val =
            min_v +
            frac * range;

        double y =
            top +
            ((double)i / 4.0) * ph;

        char txt[32];

        snprintf(
            txt,
            sizeof(txt),
            "%.0f",
            val);

        cairo_move_to(
            cr,
            8,
            y + 4);

        cairo_show_text(
            cr,
            txt);
    }


    /*
     * Graph samples
     */
    double *x_samples;
    double *y_samples;
    double *z_samples;

    if (display_mode == 0)
    {
        x_samples = acc_x;
        y_samples = acc_y;
        z_samples = acc_z;
    }
    else
    {
        x_samples = gyro_x;
        y_samples = gyro_y;
        z_samples = gyro_z;
    }


    /*
     * X = cyan
     */
    draw_axis(
        cr,
        x_samples,
        sample_count,
        min_v,
        max_v,
        left,
        top,
        pw,
        ph,
        0.20,
        0.75,
        1.00);


    /*
     * Y = green
     */
    draw_axis(
        cr,
        y_samples,
        sample_count,
        min_v,
        max_v,
        left,
        top,
        pw,
        ph,
        0.35,
        0.92,
        0.55);


    /*
     * Z = violet
     */
    draw_axis(
        cr,
        z_samples,
        sample_count,
        min_v,
        max_v,
        left,
        top,
        pw,
        ph,
        0.78,
        0.42,
        1.00);


    /*
     * Legend
     */
    cairo_set_font_size(cr, 11);

    cairo_set_source_rgb(
        cr,
        0.20,
        0.75,
        1.00);

    cairo_move_to(
        cr,
        w - 125,
        18);

    cairo_show_text(cr, "X");


    cairo_set_source_rgb(
        cr,
        0.35,
        0.92,
        0.55);

    cairo_move_to(
        cr,
        w - 85,
        18);

    cairo_show_text(cr, "Y");


    cairo_set_source_rgb(
        cr,
        0.78,
        0.42,
        1.00);

    cairo_move_to(
        cr,
        w - 45,
        18);

    cairo_show_text(cr, "Z");


    return FALSE;
}


/* =========================================================
 * Change ACC/GYRO mode
 * ========================================================= */

static void mode_changed(
    GtkToggleButton *button,
    gpointer data)
{
    if (!gtk_toggle_button_get_active(button))
        return;

    display_mode =
        GPOINTER_TO_INT(data);

    if (display_mode == 0)
    {
        gtk_label_set_text(
            GTK_LABEL(graph_title),
            "ACCELEROMETER · ±16 g");
    }
    else
    {
        gtk_label_set_text(
            GTK_LABEL(graph_title),
            "GYROSCOPE · ±2000 deg/s");
    }

    update_value_labels();

    gtk_widget_queue_draw(
        drawing_area);
}


/* =========================================================
 * Clear graph
 * ========================================================= */

static void clear_graph(
    GtkButton *button,
    gpointer data)
{
    (void)button;
    (void)data;

    memset(acc_x, 0, sizeof(acc_x));
    memset(acc_y, 0, sizeof(acc_y));
    memset(acc_z, 0, sizeof(acc_z));

    memset(gyro_x, 0, sizeof(gyro_x));
    memset(gyro_y, 0, sizeof(gyro_y));
    memset(gyro_z, 0, sizeof(gyro_z));

    sample_count = 0;

    gtk_widget_queue_draw(
        drawing_area);
}


/* =========================================================
 * Label helper
 * ========================================================= */

static GtkWidget *make_label(
    const char *text,
    const char *name)
{
    GtkWidget *label =
        gtk_label_new(text);

    gtk_widget_set_name(
        label,
        name);

    gtk_widget_set_halign(
        label,
        GTK_ALIGN_START);

    return label;
}


/* =========================================================
 * Axis card helper
 * ========================================================= */

static GtkWidget *create_axis_card(
    const char *axis,
    GtkWidget **value_out)
{
    GtkWidget *box =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            2);

    gtk_widget_set_name(
        box,
        "axis_card");

    GtkWidget *title =
        make_label(
            axis,
            "axis_title");

    GtkWidget *value =
        make_label(
            "0.000",
            "axis_value");

    gtk_box_pack_start(
        GTK_BOX(box),
        title,
        FALSE,
        FALSE,
        0);

    gtk_box_pack_start(
        GTK_BOX(box),
        value,
        TRUE,
        TRUE,
        0);

    *value_out = value;

    return box;
}


/* =========================================================
 * Main
 * ========================================================= */

int main(
    int argc,
    char **argv)
{
    gtk_init(
        &argc,
        &argv);


    /* -----------------------------------------------------
     * RPMsg
     * ----------------------------------------------------- */

    rpmsg_fd =
        open_rpmsg_auto();


    /* -----------------------------------------------------
     * CSS
     * ----------------------------------------------------- */

    GtkCssProvider *css =
        gtk_css_provider_new();

    const char *style =

        "window {"
        "  background: #070b11;"
        "}"

        "#header_title {"
        "  color: #f5f8fc;"
        "  font-size: 22px;"
        "  font-weight: 700;"
        "}"

        "#header_subtitle {"
        "  color: #728197;"
        "  font-size: 10px;"
        "}"

        "#status {"
        "  color: #5ee58a;"
        "  font-size: 12px;"
        "  font-weight: 700;"
        "}"

        "#info_card {"
        "  background: #0d141e;"
        "  border: 1px solid #182332;"
        "  border-radius: 12px;"
        "  padding: 8px 12px;"
        "}"

        "#info_title {"
        "  color: #65758b;"
        "  font-size: 9px;"
        "  font-weight: 700;"
        "}"

        "#info_value {"
        "  color: #eef3f8;"
        "  font-size: 18px;"
        "  font-weight: 700;"
        "}"

        "#small_text {"
        "  color: #8493a7;"
        "  font-size: 10px;"
        "}"

        "#axis_card {"
        "  background: #0e1621;"
        "  border: 1px solid #1b2938;"
        "  border-radius: 12px;"
        "  padding: 8px 13px;"
        "}"

        "#axis_title {"
        "  color: #6f8096;"
        "  font-size: 10px;"
        "  font-weight: bold;"
        "}"

        "#axis_value {"
        "  color: #f5f8fc;"
        "  font-size: 22px;"
        "  font-weight: 700;"
        "}"

        "#graph_title {"
        "  color: #e7edf4;"
        "  font-size: 13px;"
        "  font-weight: 700;"
        "}"

        "radiobutton {"
        "  color: #c7d1dc;"
        "  background: #101925;"
        "  border: 1px solid #1c2b3b;"
        "  border-radius: 10px;"
        "  padding: 5px 10px;"
        "}"

        "radiobutton:checked {"
        "  color: #54cfff;"
        "  background: #102838;"
        "  border-color: #245c75;"
        "}"

        "button {"
        "  color: #aebaca;"
        "  background: #111a25;"
        "  border: 1px solid #1f2c3b;"
        "  border-radius: 9px;"
        "  padding: 5px 12px;"
        "}"

        "button:hover {"
        "  background: #172433;"
        "}";


    gtk_css_provider_load_from_data(
        css,
        style,
        -1,
        NULL);

    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);


    /* -----------------------------------------------------
     * Window
     * ----------------------------------------------------- */

    GtkWidget *window =
        gtk_window_new(
            GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title(
        GTK_WINDOW(window),
        "STM32MP157 IMU Monitor");

    gtk_window_set_default_size(
        GTK_WINDOW(window),
        800,
        480);

    gtk_window_set_resizable(
        GTK_WINDOW(window),
        FALSE);

    gtk_window_set_decorated(
        GTK_WINDOW(window),
        FALSE);

    gtk_window_move(
        GTK_WINDOW(window),
        0,
        0);


    /* -----------------------------------------------------
     * Root
     * ----------------------------------------------------- */

    GtkWidget *root =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            8);

    gtk_container_set_border_width(
        GTK_CONTAINER(root),
        11);

    gtk_container_add(
        GTK_CONTAINER(window),
        root);


    /* -----------------------------------------------------
     * Header
     * ----------------------------------------------------- */

    GtkWidget *header =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            8);

    gtk_box_pack_start(
        GTK_BOX(root),
        header,
        FALSE,
        FALSE,
        0);


    GtkWidget *header_text =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            0);

    gtk_box_pack_start(
        GTK_BOX(header),
        header_text,
        TRUE,
        TRUE,
        0);


    GtkWidget *title =
        make_label(
            "STM32MP157 · IMU NODE",
            "header_title");

    GtkWidget *subtitle =
        make_label(
            "Cortex-M4  →  OpenAMP / RPMsg  →  Cortex-A7 Linux",
            "header_subtitle");

    gtk_box_pack_start(
        GTK_BOX(header_text),
        title,
        FALSE,
        FALSE,
        0);

    gtk_box_pack_start(
        GTK_BOX(header_text),
        subtitle,
        FALSE,
        FALSE,
        0);


    status_label =
        make_label(
            "● M4 OFFLINE",
            "status");

    gtk_widget_set_valign(
        status_label,
        GTK_ALIGN_CENTER);

    gtk_box_pack_start(
        GTK_BOX(header),
        status_label,
        FALSE,
        FALSE,
        8);


    /* -----------------------------------------------------
     * Top information cards
     * ----------------------------------------------------- */

    GtkWidget *info_row =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            8);

    gtk_box_pack_start(
        GTK_BOX(root),
        info_row,
        FALSE,
        FALSE,
        0);


    /*
     * Rate
     */
    GtkWidget *rate_card =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            1);

    gtk_widget_set_name(
        rate_card,
        "info_card");

    gtk_box_pack_start(
        GTK_BOX(rate_card),
        make_label(
            "UPDATE RATE",
            "info_title"),
        FALSE,
        FALSE,
        0);

    rate_label =
        make_label(
            "0 Hz",
            "info_value");

    gtk_box_pack_start(
        GTK_BOX(rate_card),
        rate_label,
        FALSE,
        FALSE,
        0);

    gtk_box_pack_start(
        GTK_BOX(info_row),
        rate_card,
        TRUE,
        TRUE,
        0);


    /*
     * Packets
     */
    GtkWidget *packet_card =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            1);

    gtk_widget_set_name(
        packet_card,
        "info_card");

    gtk_box_pack_start(
        GTK_BOX(packet_card),
        make_label(
            "RX PACKETS",
            "info_title"),
        FALSE,
        FALSE,
        0);

    packet_label =
        make_label(
            "0 packets",
            "info_value");

    gtk_box_pack_start(
        GTK_BOX(packet_card),
        packet_label,
        FALSE,
        FALSE,
        0);

    gtk_box_pack_start(
        GTK_BOX(info_row),
        packet_card,
        TRUE,
        TRUE,
        0);


    /*
     * Device
     */
    GtkWidget *device_card =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            1);

    gtk_widget_set_name(
        device_card,
        "info_card");

    gtk_box_pack_start(
        GTK_BOX(device_card),
        make_label(
            "RPMSG DEVICE",
            "info_title"),
        FALSE,
        FALSE,
        0);

    device_label =
        make_label(
            rpmsg_device,
            "small_text");

    gtk_box_pack_start(
        GTK_BOX(device_card),
        device_label,
        TRUE,
        TRUE,
        0);

    gtk_box_pack_start(
        GTK_BOX(info_row),
        device_card,
        TRUE,
        TRUE,
        0);


    /* -----------------------------------------------------
     * XYZ values
     * ----------------------------------------------------- */

    GtkWidget *axis_row =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            8);

    gtk_box_pack_start(
        GTK_BOX(root),
        axis_row,
        FALSE,
        FALSE,
        0);


    GtkWidget *x_card =
        create_axis_card(
            "X AXIS",
            &x_value_label);

    GtkWidget *y_card =
        create_axis_card(
            "Y AXIS",
            &y_value_label);

    GtkWidget *z_card =
        create_axis_card(
            "Z AXIS",
            &z_value_label);


    gtk_box_pack_start(
        GTK_BOX(axis_row),
        x_card,
        TRUE,
        TRUE,
        0);

    gtk_box_pack_start(
        GTK_BOX(axis_row),
        y_card,
        TRUE,
        TRUE,
        0);

    gtk_box_pack_start(
        GTK_BOX(axis_row),
        z_card,
        TRUE,
        TRUE,
        0);


    /* -----------------------------------------------------
     * Graph controls
     * ----------------------------------------------------- */

    GtkWidget *graph_controls =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            8);

    gtk_box_pack_start(
        GTK_BOX(root),
        graph_controls,
        FALSE,
        FALSE,
        0);


    graph_title =
        make_label(
            "ACCELEROMETER · ±16 g",
            "graph_title");

    gtk_box_pack_start(
        GTK_BOX(graph_controls),
        graph_title,
        TRUE,
        TRUE,
        0);


    accel_button =
        gtk_radio_button_new_with_label(
            NULL,
            "ACCEL");

    gyro_button =
        gtk_radio_button_new_with_label_from_widget(
            GTK_RADIO_BUTTON(accel_button),
            "GYRO");

    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(accel_button),
        TRUE);


    g_signal_connect(
        accel_button,
        "toggled",
        G_CALLBACK(mode_changed),
        GINT_TO_POINTER(0));

    g_signal_connect(
        gyro_button,
        "toggled",
        G_CALLBACK(mode_changed),
        GINT_TO_POINTER(1));


    gtk_box_pack_start(
        GTK_BOX(graph_controls),
        accel_button,
        FALSE,
        FALSE,
        0);

    gtk_box_pack_start(
        GTK_BOX(graph_controls),
        gyro_button,
        FALSE,
        FALSE,
        0);


    GtkWidget *clear_button =
        gtk_button_new_with_label(
            "CLEAR");

    g_signal_connect(
        clear_button,
        "clicked",
        G_CALLBACK(clear_graph),
        NULL);

    gtk_box_pack_start(
        GTK_BOX(graph_controls),
        clear_button,
        FALSE,
        FALSE,
        0);


    /* -----------------------------------------------------
     * Graph
     * ----------------------------------------------------- */

    drawing_area =
        gtk_drawing_area_new();

    gtk_widget_set_size_request(
        drawing_area,
        -1,
        210);

    gtk_box_pack_start(
        GTK_BOX(root),
        drawing_area,
        TRUE,
        TRUE,
        0);


    g_signal_connect(
        drawing_area,
        "draw",
        G_CALLBACK(draw_graph),
        NULL);


    /* -----------------------------------------------------
     * Window signals
     * ----------------------------------------------------- */

    g_signal_connect(
        window,
        "destroy",
        G_CALLBACK(gtk_main_quit),
        NULL);


    /* -----------------------------------------------------
     * Timers
     * ----------------------------------------------------- */

    g_timeout_add(
        5,
        update_data,
        NULL);

    g_timeout_add(
        1000,
        update_rate,
        NULL);


    gtk_widget_show_all(
        window);

    gtk_main();


    if (rpmsg_fd >= 0)
        close(rpmsg_fd);

    return 0;
}