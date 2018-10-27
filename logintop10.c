/*
 * $Log: logintop10.c,v $
 * Revision 1.6  2003/04/03 18:06:38  folkert
 * *** empty log message ***
 *
 * Revision 1.5  2003/04/03 18:04:31  folkert
 * added
 * raw output didn't work (not accepted on cmd line)
 * added xx:xx:xx format for duration
 *
 * Revision 1.4  2003/01/28 20:21:17  folkert
 * small locale-fix
 *
 * Revision 1.3  2003/01/28 18:34:50  folkert
 * added locale-stuff
 *
 * Revision 1.2  2003/01/27 17:29:54  folkert
 * Added per-user statistics
 *
 */
char *version = "$Id: logintop10.c,v 1.7 2003/04/03 18:06:38 folkert Exp folkert $";

#include <locale.h>
#include <libintl.h>
#define _(str) gettext(str)	/* libintl */
#include <stdlib.h>
#include <stdio.h>
#include <utmp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "io.h"
#include "mem.h"
#include "val.h"

typedef struct
{
	char ut_user[UT_NAMESIZE+1];
	char ut_line[UT_LINESIZE+1];
	time_t login_start, login_end;
	int value;
}
record;

typedef struct
{
	char username[UT_NAMESIZE+1];
	unsigned long int n_logins;
	unsigned long int d_min, d_max, d_average;  /* duration */
	unsigned long int av_hour_in, av_hour_out;  /* average login/out time */
	unsigned long int day[7];                   /* favorite login-day */
}
user_record;

int process_file(char **filenames, record **data, int *n_records)
{	
	int fd;
	int filename_count = 0;
	struct utmp *spool = NULL;
	int n_spool=0, spool_size=0;

	int records_size=0;

	*data = NULL;
	*n_records = 0;

	do
	{
		fd = open(filenames[filename_count], O_RDONLY);
		if (fd == -1)
		{
			printf(_("Problem loading data from %s.\n"), filenames[filename_count]);
			return -1;
		}

		for(;;)
		{
			struct utmp in;

			if (READ(fd, (char *)&in, sizeof(in)) < sizeof(in))
				break;

			if (in.ut_type == RUN_LVL || in.ut_type == BOOT_TIME)
			{
				free(spool);
				spool = NULL;
				n_spool = spool_size = 0;
			}

			/* printf("%s %s %d\n", in.ut_user, in.ut_line, in.ut_type); */

			if (strcmp(in.ut_line, "|") == 0 ||
			    strcmp(in.ut_line, "}") == 0)
				continue;

			if (in.ut_type != USER_PROCESS && in.ut_type != DEAD_PROCESS)
				continue;

			if (strlen(in.ut_user) == 0) /* logout? */
			{
				int loop;

				for(loop=0; loop<n_spool; loop++)
				{
					if (strcmp(in.ut_line, spool[loop].ut_line) == 0)
					{
						if (resize((void **)data, *n_records, &records_size, sizeof(record)) == -1)
							exit(1);

						(*data)[*n_records].login_start = spool[loop].ut_tv.tv_sec;
						(*data)[*n_records].login_end = in.ut_tv.tv_sec;
						strncpy((*data)[*n_records].ut_user, spool[loop].ut_user, UT_NAMESIZE);
						((*data)[*n_records].ut_user)[UT_NAMESIZE] = 0x00;
						strncpy((*data)[*n_records].ut_line, spool[loop].ut_line, UT_NAMESIZE);
						((*data)[*n_records].ut_line)[UT_LINESIZE] = 0x00;
						(*n_records)++;

						spool[loop].ut_pid = 0;
						break;
					}
				}
			}
			else
			{
				int loop;

				for(loop=0; loop<n_spool; loop++)
				{
					if (spool[loop].ut_pid == 0)
					{
						spool[loop] = in;
						break;
					}
				}

				if (loop == n_spool)
				{
					// add
					if (resize((void **)&spool, n_spool, &spool_size, sizeof(struct utmp)) == -1)
						exit(1);

					spool[n_spool++] = in;
				}
			}
		}

		close(fd);
	}
	while(filenames[++filename_count]);

	return 0;
}

void tr_color(FILE *fh, char *color)
{
	if (*color)
	{
		*color = 0;
		fprintf(fh, "<TR BGCOLOR=\"#EECCAA\">");
	}
	else
	{
		*color = 1;
		fprintf(fh, "<TR BGCOLOR=\"#AACCEE\">");
	}
}

char * str_duration(int n)
{
	int hour = 0, min = 0;
	char *dummy = (char *)malloc(128);
	if (!dummy)
		return NULL;

	if (n > 3600)
	{
		hour = (n / 3600);
		n -= hour * 3600;
	}

	if (n > 60)
	{
		min = n / 60;
		n -= min * 60;
	}

	sprintf(dummy, "%2d:%02d:%02d", hour, min, n);

	return dummy;
}

int output_data(record *data, int n_records, char raw, char *filename)
{
	int loop;
	unsigned long int average_duration = 0, min_duration = 0x7fffffff, max_duration = 0;
	record *top10_lf = NULL; /* login frequency */
	int top10_lf_n = 0, top10_lf_size = 0;
	record *top10_d = NULL;  /* top 10 durations */
	int top10_d_n = 0, top10_d_size = 0;
	unsigned long int busiest_week_day[7], busiest_month_day[31+1], busiest_month[12], bwd_i=0, bwd=0, bmd_i=0, bmd=0, bm_i=0, bm=0;
	unsigned long int qwd_i=0, qwd=0x7fffffff, qmd_i=0, qmd=0x7fffffff, qm_i=0, qm=0x7fffffff;
	char *day_of_week[] = {_("Sunday"), _("Monday"), _("Tuesday"), _("Wednesday"), _("Thursday"), _("Friday"), _("Saturday")};
	char *month[] = {_("January"), _("February"), _("March"), _("April"), _("May"), _("June"), _("July"), _("August"), _("September"), _("October"), _("November"), _("December")};
	FILE *fh;
	char color=0;
	time_t now;
	
	user_record *ur = NULL;
	int n_ur=0, ur_size=0;

	memset(busiest_week_day, 0x00, sizeof(busiest_week_day));
	memset(busiest_month_day, 0x00, sizeof(busiest_month_day));

	fh = fopen(filename, "w");
	if (!fh)
		return -1;

	fprintf(fh, _("<H1>Login/logout statistics</H1>\n"));

	fprintf(fh, _("<H2>Overall data</H2>\n"));
	fprintf(fh, "<UL>\n");
	time(&now);
	fprintf(fh, _("<LI>Last run: %s\n"), ctime(&now));
	fprintf(fh, _("<LI>Number of records: %d\n"), n_records);

	for(loop=0; loop<n_records; loop++)
	{
		int loop2;
		int duration = data[loop].login_end - data[loop].login_start;
		struct tm *p_tm;
		user_record *cur_user = NULL;

		average_duration += duration;
		if (duration < min_duration)
			min_duration = duration;
		if (duration > max_duration)
			max_duration = duration;

		/* login frequency */
		if (resize((void **)&top10_lf, top10_lf_n, &top10_lf_size, sizeof(record)) == -1)
			exit(1);
		for(loop2=0; loop2<top10_lf_n; loop2++)
		{
			if (strcmp(top10_lf[loop2].ut_user, data[loop].ut_user) == 0)
			{
				top10_lf[loop2].value++;
				break;
			}
		}
		if (loop2 == top10_lf_n)
		{
			top10_lf[top10_lf_n] = data[loop];
			top10_lf[top10_lf_n++].value = 0;
		}

		/* duration top10 */
		if (resize((void **)&top10_d, top10_d_n, &top10_d_size, sizeof(record)) == -1)
			exit(1);
		for(loop2=0; loop2<top10_d_n; loop2++)
		{
			if (strcmp(top10_d[loop2].ut_user, data[loop].ut_user) == 0)
			{
				int duration = data[loop].login_end - data[loop].login_start;
				if (duration > top10_d[loop2].value)
				{
					top10_d[loop2].value = duration;
				}
				break;
			}
		}
		if (loop2 == top10_d_n)
		{
			top10_d[top10_d_n] = data[loop];
			top10_d[top10_d_n++].value = 0;
		}

		p_tm = localtime(&(data[loop].login_start));

		/* busiest weekday */
		busiest_week_day[p_tm -> tm_wday]++;

		/* busiest monthday */
		busiest_month_day[p_tm -> tm_mday]++;

		/* busiest month */
		busiest_month[p_tm -> tm_mon]++;

		/* per user info */
		for(loop2=0; loop2<n_ur; loop2++)
		{
			if (strcmp(ur[loop2].username, data[loop].ut_user) == 0)
			{
				cur_user = &ur[loop2];
				break;
			}
		}
		if (loop2 == n_ur) /* not found? add */
		{
			if (resize((void **)&ur, n_ur, &ur_size, sizeof(user_record)) == -1)
				exit(1);

			memset(&ur[n_ur], 0x00, sizeof(user_record));
			strcpy(ur[n_ur].username, data[loop].ut_user);
			ur[n_ur].d_min = 0x7fffffff;

			cur_user = &ur[n_ur];
			n_ur++;
		}
		if (cur_user)
		{
			struct tm *p2_tm;
			(cur_user -> day)[p_tm -> tm_wday]++;
			if (duration < cur_user -> d_min)
				cur_user -> d_min = duration;
			if (duration > cur_user -> d_max)
				cur_user -> d_max = duration;
			cur_user -> d_average += duration;
			cur_user -> n_logins++;
			cur_user -> av_hour_in += p_tm -> tm_hour;
			p2_tm = localtime(&(data[loop].login_end));
			cur_user -> av_hour_out += p2_tm -> tm_hour;
		}
	}

	/* sort top10 login frequency */
	for(loop=0; loop<(top10_lf_n-1); loop++)
	{
		int loop2;

		for(loop2=loop+1; loop2<top10_lf_n; loop2++)
		{
			if (top10_lf[loop2].value > top10_lf[loop].value)
			{
				record dummy = top10_lf[loop];
				top10_lf[loop] = top10_lf[loop2];
				top10_lf[loop2] = dummy;
			}
		}
	}

	/* sort top10 duration */
	for(loop=0; loop<(top10_d_n-1); loop++)
	{
		int loop2;

		for(loop2=loop+1; loop2<top10_d_n; loop2++)
		{
			if (top10_d[loop2].value > top10_d[loop].value)
			{
				record dummy = top10_d[loop];
				top10_d[loop] = top10_d[loop2];
				top10_d[loop2] = dummy;
			}
		}
	}

	fprintf(fh, _("<LI>Min. duration: %ld\n"), min_duration);
	fprintf(fh, _("<LI>Max. duration: %ld\n"), max_duration);
	fprintf(fh, _("<LI>Average duration: %ld\n"), average_duration / n_records);
	fprintf(fh, "</UL>\n");

	fprintf(fh, _("<H2>Top 10 Login Frequency</H2>\n"));
	fprintf(fh, _("<TABLE BORDER=1>\n"));
	fprintf(fh, _("<TR><TD></TD><TD><B>Username:</B></TD><TD><B>Count</B></TD></TR>\n"));
	for(loop=0; loop<min(top10_lf_n, 10); loop++)
	{
		tr_color(fh, &color);
		fprintf(fh, _("<TD>%d</TD><TD>%s</TD><TD>%d</TD></TR>\n"), loop+1, top10_lf[loop].ut_user, top10_lf[loop].value);
	}
	fprintf(fh, _("</TABLE>\n"));

	fprintf(fh, _("<H2>Top 10 Login Duration</H2>\n"));
	fprintf(fh, _("<TABLE BORDER=1>\n"));
	fprintf(fh, _("<TR><TD></TD><TD><B>Username:</B></TD><TD><B>Duration</B></TD></TR>\n"));
	for(loop=0; loop<min(top10_d_n, 10); loop++)
	{
		char *d = str_duration(top10_d[loop].value);
		tr_color(fh, &color);
		fprintf(fh, "<TD>%d</TD><TD>%s</TD><TD>%s</TD></TR>\n", loop+1, top10_d[loop].ut_user, d);
		free(d);
	}
	fprintf(fh, "</TABLE>\n");

	fprintf(fh, _("<H2>Busiest day of the Week</H2>\n"));
	fprintf(fh, "<UL>\n");
	fprintf(fh, "<LI>");
	for(loop=0; loop<7; loop++)
	{
		if (busiest_week_day[loop] > bwd)
		{
			bwd = busiest_week_day[loop];
			bwd_i = loop;
		}
		if (busiest_week_day[loop] < qwd)
		{
			qwd = busiest_week_day[loop];
			qwd_i = loop;
		}
	}
	fprintf(fh, _("%s (%ld logins)\n"), day_of_week[bwd_i], bwd);
	fprintf(fh, _("<LI>Most quiet day: %s (%ld logins)\n"), day_of_week[qwd_i], qwd);
	fprintf(fh, "</UL>\n");
	fprintf(fh, "<TABLE BORDER=1>\n");
	fprintf(fh, "<TR><TD><B>Day</B></TD><TD><B>Number of logins</B></TD></TR>\n");
	for(loop=0; loop<7; loop++)
	{
		tr_color(fh, &color);
		fprintf(fh, "<TD>%s</TD><TD>%ld</TD></TR>\n", day_of_week[loop], busiest_week_day[loop]);
	}
	fprintf(fh, "</TABLE>\n");

	fprintf(fh, _("<H2>Busiest day of the Month</H2>\n"));
	fprintf(fh, "<UL>\n");
	fprintf(fh, "<LI>");
	for(loop=1; loop<32; loop++)
	{
		if (busiest_month_day[loop] > bmd)
		{
			bmd = busiest_month_day[loop];
			bmd_i = loop;
		}
		if (busiest_month_day[loop] < qmd)
		{
			qmd = busiest_month_day[loop];
			qmd_i = loop;
		}
	}
	fprintf(fh, _("Day %ld (%ld logins)\n"), bmd_i, bmd);
	fprintf(fh, _("<LI>Most quiet day: %ld (%ld logins)\n"), qmd_i, qmd);
	fprintf(fh, "</UL>\n");
	fprintf(fh, "<TABLE BORDER=1>\n");
	fprintf(fh, _("<TR><TD><B>Day</B></TD><TD><B>Number of logins</B></TD></TR>\n"));
	for(loop=1; loop<32; loop++)
	{
		tr_color(fh, &color);
		fprintf(fh, "<TD>%d</TD><TD>%ld</TD></TR>\n", loop, busiest_month_day[loop]);
	}
	fprintf(fh, "</TABLE>\n");

	fprintf(fh, _("<H2>Busiest Month</H2>\n"));
	fprintf(fh, "<UL>\n");
	fprintf(fh, "<LI>");
	for(loop=0; loop<12; loop++)
	{
		if (busiest_month[loop] > bm)
		{
			bm = busiest_month[loop];
			bm_i = loop;
		}
		if (busiest_month[loop] < qm)
		{
			qm = busiest_month[loop];
			qm_i = loop;
		}
	}
	fprintf(fh, _("%s (%ld logins)\n"), month[bm_i], bm);
	fprintf(fh, _("<LI>Most quiet month: %s (%ld logins)\n"), month[qm_i], qm);
	fprintf(fh, "</UL>\n");
	fprintf(fh, "<TABLE BORDER=1>\n");
	fprintf(fh, _("<TR><TD><B>Month</B></TD><TD><B>Number of logins</B></TD></TR>\n"));
	for(loop=0; loop<12; loop++)
	{
		tr_color(fh, &color);
		fprintf(fh, "<TD>%s</TD><TD>%ld</TD></TR>\n", month[loop], busiest_month[loop]);
	}
	fprintf(fh, "</TABLE>\n");

	fprintf(fh, _("<H2>Per user statistics</H2>\n"));
	fprintf(fh, "<TABLE BORDER=1>\n");
	fprintf(fh, _("<TR><TD><B>Username:</B></TD><TD><B>Number of logins:</B></TD><TD><B>Min. duration</B></TD><TD><B>Max. duration</B></TD><TD><B>Average Duration</B></TD><TD><B>Av. login hour</B></TD><TD><B>Av. logout hour</B></TD><TD><B>Favorite day</B></TD></TR>\n"));
	for(loop=0; loop<n_ur; loop++)
	{
		int loop2, f_d_i=0;
		unsigned long int f_d=0;
		char *d1 = str_duration(ur[loop].d_min), *d2 = str_duration(ur[loop].d_max), *d3 = str_duration((double)ur[loop].d_average/(double)ur[loop].n_logins);

		tr_color(fh, &color);
		fprintf(fh, "<TD>%s</TD><TD>%ld</TD><TD>%s</TD><TD>%s</TD><TD>%s</TD><TD>%ld</TD><TD>%ld</TD>",
				ur[loop].username, ur[loop].n_logins, d1, d2, d3, ur[loop].av_hour_in/ur[loop].n_logins, ur[loop].av_hour_out/ur[loop].n_logins);

		free(d3);
		free(d2);
		free(d1);

		for(loop2=0; loop2<7; loop2++)
		{
			if (ur[loop].day[loop2] > f_d)
			{
				f_d = ur[loop].day[loop2];
				f_d_i = loop2;
			}
		}

		fprintf(fh, _("<TD>%s (%ld logins)</TD></TR>\n"), day_of_week[f_d_i], f_d);
	}
	fprintf(fh, "</TABLE>\n");

	if (raw)
	{
		fprintf(fh, "<BR>\n");
		fprintf(fh, "<BR>\n");
		fprintf(fh, "<HR>\n");
		fprintf(fh, _("<H2>Raw Data</H2>\n"));
		fprintf(fh, "<TABLE BORDER=1>\n");
		fprintf(fh, _("<TR><TD><B>Username</B></TD><TD><B>TTY</B></TD><TD><B>Logged in at:</B></TD><TD><B>Duration:</B></TD></TR>\n"));
		for(loop=0; loop<n_records; loop++)
		{
			char *d = str_duration(data[loop].login_end - data[loop].login_start);
			tr_color(fh, &color);
			fprintf(fh, "<TD>%s</TD><TD>%s</TD><TD>%s</TD><TD>%s</TD></TR>\n", 
					data[loop].ut_user, 
					data[loop].ut_line, 
					ctime(&(data[loop].login_start)), 
					d);
			free(d);
		}
		fprintf(fh, "</TABLE>\n");
	}

	fprintf(fh, "<HR>\n");
	fprintf(fh, _("<FONT SIZE=-1>Created with <A HREF=\"http://www.vanheusden.com/logintop10/\">logintop10</A> (written by <A HREF=\"mailto:folkert@vanheusden.com\">folkert@vanheusden.com</A>).</FONT>\n"));

	fclose(fh);

	return 0;
}

int main(int argc, char *argv[])
{
	record *data;
	int n_records;
	int c;
	int inputs_sz = 2;
	char **inputs = NULL, *output = NULL, *l = NULL, *el="";
	char help = 0, raw = 0;

	while((c = getopt(argc, argv, "ri:o:l:h")) != -1)
	{
		switch(c)
		{
		case 'i':
			if (!inputs)
			{
				inputs = malloc(sizeof(char *) * 2);
				if (!inputs)
				{
					fprintf(stderr, _("resize: Cannot (re-)allocate %d bytes of memory\n"), sizeof(char *) * 2);
					return 1;
				}
				inputs_sz = 2;
				inputs[0] = optarg;
				inputs[1] = NULL;
			}
			else
			{
				++inputs_sz;
				inputs = realloc(inputs, sizeof(char *) * inputs_sz);
				if (!inputs)
				{
					fprintf(stderr, _("resize: Cannot (re-)allocate %d bytes of memory\n"), sizeof(char *) * inputs_sz);
					return 1;
				}
				inputs[inputs_sz - 2] = optarg;
				inputs[inputs_sz - 1] = NULL;
			}
			break;
		case 'o':
			output = optarg;
			break;
		case 'l':
			l = optarg;
			break;
		case 'r':
			raw = 1;
			break;
		case 'h':
			help = 1;
			break;
		default:
			help = 1;
			break;
		}
	}

	if (l == NULL)
	{
		l = el;
	}

	setlocale(LC_ALL, l);
	textdomain("logintop10");

	printf(_("logintop10, (c) 2003 by folkert@vanheusden.com\n"));
	printf(_("Version: %s\n\n"), version);

	if (inputs == NULL || output == NULL)
	{
		help = 1;
	}

	if (help)
	{
		printf(_("Usage: %s -i wtmp -o fileout.html [-l locale] [-r]\n"), argv[0]);
		printf(_("Example: logintop10 -i /var/log/wtmp -o ~/www/logintop10.html -l nl_NL.utf8\n"));
		return 1;
	}

	if (process_file(inputs, &data, &n_records) != 0)
	{
		printf(_("Problem loading data.\n"));
		return 1;
	}

	if (output_data(data, n_records, raw, output) != 0)
	{
		printf(_("Problem saving data to %s.\n"), output);
		return 1;
	}

	return 0;
}
