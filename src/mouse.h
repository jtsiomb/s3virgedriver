#ifndef MOUSE_H_
#define MOUSE_H_

int reset_mouse(void);
unsigned int read_mouse(int *xp, int *yp);
void set_mouse_xrange(int xmin, int xmax);
void set_mouse_yrange(int ymin, int ymax);

#endif	/* MOUSE_H_ */
