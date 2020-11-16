#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
 
#define CAMERA_CLEAR(x) memset(&(x), 0, sizeof(x))


uint8_t *buffer;

#define WID 1920
#define HEI 1080


static int camera_input_set(int fd, unsigned int idx)
{
	int ret;
	struct v4l2_input input;
	CAMERA_CLEAR(input);
	input.index = idx;
	input.type = V4L2_INPUT_TYPE_CAMERA;
  ret = ioctl(fd, VIDIOC_S_INPUT, &input);
	return ret;
}

static int camera_param_set(int fd, unsigned int cap_mode, int fps)
{
  int ret = 0;
  struct v4l2_streamparm streamparam;
  CAMERA_CLEAR(streamparam);
  streamparam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  streamparam.parm.capture.timeperframe.numerator = 1;
  streamparam.parm.capture.timeperframe.denominator = fps;
  streamparam.parm.capture.capturemode = cap_mode;
  ret = ioctl(fd, VIDIOC_S_PARM, &streamparam);
  return ret;
}

static int camera_fmt_set(int fd , unsigned int width, unsigned int height)
{
  int ret;
  struct v4l2_format format;
  CAMERA_CLEAR(format);
  format.type                	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
  format.fmt.pix.width       	= width;
  format.fmt.pix.height      	= height;
  format.fmt.pix.pixelformat 	= V4L2_PIX_FMT_YUV420;
  format.fmt.pix.field       	= V4L2_FIELD_NONE;
  ret = ioctl(fd, VIDIOC_S_FMT, &format);
  return ret;
}

static int camera_buf_request(int fd, unsigned int count)
{

  int ret;
  unsigned int i;
  struct v4l2_requestbuffers requestbuffers;
  struct v4l2_buffer v4l2Buf; 	
  CAMERA_CLEAR(requestbuffers);
  requestbuffers.count	= count;
  requestbuffers.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
  requestbuffers.memory	= V4L2_MEMORY_MMAP;	
  ret = ioctl(fd, VIDIOC_REQBUFS, &requestbuffers);
  if(ret != 0)
  {
    printf("%s:ioctl v4l2 buf request error!, ret = %d, count = %d\n", __func__, ret, requestbuffers.count);
    goto errHdl;
  }
	
  printf("%s:count %d\n", __func__,requestbuffers.count);
  for(i = 0; i < requestbuffers.count; i++){
    CAMERA_CLEAR(v4l2Buf);
    v4l2Buf.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2Buf.memory		= V4L2_MEMORY_MMAP;
    v4l2Buf.index		= i;
    ret = ioctl(fd, VIDIOC_QUERYBUF, &v4l2Buf);
    if(ret != 0)
    {
      printf("%s:ioctl v4l2 buf query error, ret = %d!\n", __func__, ret);
      goto errHdl;
    }
    buffer = mmap(NULL, v4l2Buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, v4l2Buf.m.offset);
    if(buffer == MAP_FAILED)
    {
      printf("%s:mmap kernel space alloc buf to user space error!\n", __func__);
      goto errHdl;
    }
    printf("Length: %d\nAddress: %p\n",  v4l2Buf.length, buffer);
    printf("Image Length: %d\n", v4l2Buf.bytesused);


    v4l2Buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2Buf.memory	= V4L2_MEMORY_MMAP;
    v4l2Buf.index	= i;
    ret = ioctl(fd, VIDIOC_QBUF, &v4l2Buf);
    if(ret != 0)
    {
      printf("%s:ioctl v4l2 buf enqueue error!\n", __func__);
      goto errHdl;
    }
  }
	
errHdl:
  return ret;
}

int main(int argc,char* argv[]){

  if(4 != argc){
    printf("./func /dev/video_X width height\n");
    return -1;
  }

  unsigned int wid = (unsigned int)atoi(argv[2]); 
  unsigned int hei = (unsigned int)atoi(argv[3]);

  int fd,input,ret;
  //open device
  //fd = open("/dev/video0", O_RDWR);
  fd = open(argv[1], O_RDWR);
  if (fd == -1)
  {
    printf("Opening vid device %s failed!!\n",argv[1]);
    return -1;
  }
  // set input
  for (input = 0; input < 4; input++) {
    ret = camera_input_set(fd, input);
    if(!ret){
      printf("%s: set csi input %d OK!\n",__func__,input);
      break;
    }else{
      printf("%s: set csi input %d FAIL!\n",__func__,input);
    }
  }
  //set capture mode 
  ret = camera_param_set(fd, 0x3 ,20);
  if(ret != 0)
  {
    printf("set csi stream param error!\n");
    return -1;
  }
  
  printf("[%s] set_mt w : %u , h : %u\n",__func__,wid,hei);

  ret = camera_fmt_set(fd, wid, hei);
  if(ret != 0)
  {
    printf("set csi res and fmt error!\n");
    return -1;
  }

  ret = camera_buf_request(fd,1);
  if(ret != 0)
  {
    printf("camera_buf_request error!\n");
    return -1;
  }

  enum v4l2_buf_type type;
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  ret = ioctl(fd,VIDIOC_STREAMON,&type);
  if(ret != 0)
  {
    printf("VIDIOC_STREAMON error!\n");
    return -1;
  }

  struct timespec start, stop;

  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);

  //for(;;){
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(fd, &fds);
  struct timeval tv = {0};
  tv.tv_sec = 2;

  ret = select(fd+1, &fds, NULL, NULL, &tv);

  if(-1 == ret)
  {
    printf("select camera error,ret %d\n",ret);
    return -1;
  }else if(0 == ret){
    printf("select camera timeout,ret %d\n",ret);
	return -1;
  }

  printf("select succ . %d\n");

  struct v4l2_buffer v4l2Buf;

  CAMERA_CLEAR(v4l2Buf);

  v4l2Buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  v4l2Buf.memory = V4L2_MEMORY_MMAP;
  ret = ioctl(fd, VIDIOC_DQBUF, &v4l2Buf);

  if(-1 == ret){
    printf("Retrieving Frame 1");
    return -1;
  }

  //}

  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &stop);

  long long unsigned elapsed = (stop.tv_sec - start.tv_sec)*1e9 + stop.tv_nsec - start.tv_nsec;

  printf("time elapsed %llu ns %4.3f ms\n",elapsed ,elapsed/1000000.0);    // in microseconds

  FILE *fp = fopen("./out.img", "wb");
  if(fp) fwrite(buffer, v4l2Buf.bytesused,1,fp);
  fflush(fp);
  fclose(fp);
 
  close(fd);

  return 0;
}
