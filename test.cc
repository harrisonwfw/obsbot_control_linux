#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#define NBUF 3

void query_capabilites(int fd)
{
	struct v4l2_capability cap;

	if (-1 == ioctl(fd, VIDIOC_QUERYCAP, &cap)) {
		perror("Query capabilites");
		exit(EXIT_FAILURE);
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf(stderr, "Device is no video capture device\\n");
		exit(EXIT_FAILURE);
	}

	if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
		fprintf(stderr, "Device does not support read i/o\\n");
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "Devices does not support streaming i/o\\n");
		exit(EXIT_FAILURE);
	}
}

int set_format(int fd)
{
	struct v4l2_format format = {0};
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.width = 320;
	format.fmt.pix.height = 240;
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	format.fmt.pix.field = V4L2_FIELD_NONE;

	int res = ioctl(fd, VIDIOC_S_FMT, &format);
	if(res == -1) {
		perror("Could not set format");
		exit(EXIT_FAILURE);
	}

	return res;
}

int request_buffer(int fd, int count)
{
	struct v4l2_requestbuffers req = {0};
	req.count = count;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
		perror("Requesting Buffer");
		exit(EXIT_FAILURE);
	}

	return req.count;
}

int query_buffer(int fd, int index, unsigned char **buffer)
{
	struct v4l2_buffer buf = {0};
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = index;

	if(ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
		perror("Could not query buffer");
		exit(EXIT_FAILURE);
	}

	*buffer = (u_int8_t*)mmap (NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
	return buf.length;
}

int queue_buffer(int fd, int index)
{
	struct v4l2_buffer bufd = {0};
	bufd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	bufd.memory = V4L2_MEMORY_MMAP;
	bufd.index = index;

	if (ioctl(fd, VIDIOC_QBUF, &bufd) == -1) {
		perror("Queue Buffer");
		exit(EXIT_FAILURE);
	}

	return bufd.bytesused;
}

int start_streaming(int fd)
{
	unsigned int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
		perror("VIDIOC_STREAMON");
		exit(EXIT_FAILURE);
	}

	return 0;
}

int dequeue_buffer(int fd)
{
	struct v4l2_buffer bufd = {0};
	bufd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	bufd.memory = V4L2_MEMORY_MMAP;
	bufd.index = 0;

	if (ioctl(fd, VIDIOC_DQBUF, &bufd) == -1) {
		perror("DeQueue Buffer");
		exit(EXIT_FAILURE);
	}

	return bufd.index;
}


int stop_streaming(int fd)
{
	unsigned int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(ioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
		perror("VIDIOC_STREAMON");
		exit(EXIT_FAILURE);
	}

	return 0;
}

int main()
{
	unsigned char *buffer[NBUF];
	int fd = open("/dev/video4", O_RDWR);
	int size;
	int index;
	int nbufs;

	/* Step 1: Query capabilities */
	query_capabilites(fd);

	/* Step 2: Set format */
	set_format(fd);

	/* Step 3: Request Format */
	nbufs = request_buffer(fd, NBUF);
	if ( nbufs > NBUF) {
		fprintf(stderr, "Increase NBUF to at least %i\n", nbufs);
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < NBUF; i++) {
		/* Step 4: Query buffers 
		 * Assume all sizes is equal.
		 * */
		size = query_buffer(fd, i, &buffer[i]);

		/* Step 5: Queue buffer */
		queue_buffer(fd, i);
	}

	/* Step 6: Start streaming */
	start_streaming(fd);

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	struct timeval tv = {0};
	tv.tv_sec = 2;
	int r = select(fd+1, &fds, NULL, NULL, &tv);
	if(-1 == r){
		perror("Waiting for Frame");
		exit(1);
	}

	/* Step 7: Dequeue buffer */
	index = dequeue_buffer(fd);

	int file = open("output.raw", O_RDWR | O_CREAT, 0666);
	write(file, buffer[index], size);

	/* Step 8: Stop streaming */
	stop_streaming(fd);


	/* Cleanup the resources */
	for (int i =0; i < NBUF; i++) {
		munmap(buffer[i], size);
	}
	close(file);
	close(fd);

	return 0;
}