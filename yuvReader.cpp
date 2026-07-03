#include "yuvReader.h"


#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "YuvReader"

static char *get_format_name(unsigned int format);
static int isOutputRawData(int format);
static int getSensorType(int fd);
static int getSensorGain(int fd);
static int getSensorExp(int fd);

YuvReader::YuvReader()
{

}

YuvReader::~YuvReader()
{

}

int YuvReader::Init(int w, int h, int fps)
{
	char camera_path[16];
	struct v4l2_capability cap;      /* Query device capabilities */
	struct v4l2_fmtdesc fmtdesc;     /* Enumerate image formats */
	struct v4l2_frmsizeenum frmsize; /* Enumerate frame sizes */
	struct v4l2_frmivalenum ival;	/* Enumerate fps */
	struct v4l2_format fmt;          /* try a format */
	struct v4l2_input inp;           /* select the current video input */
	struct v4l2_streamparm parms;    /* set streaming parameters */
	 /* Initiate Memory Mapping or User Pointer I/O */
	          /* Query the status of a buffer */

	int n_buffers = 0;
	int index = 0;

	LOGI("start");
	/* default settings */
	memset(&camera, 0, sizeof(camera_handle));
	memset(camera_path, 0, sizeof(camera_path));

	camera.camera_index = 0;
	//camera.isDefault = true;
	camera.sensor_type = -1;
	camera.ispId = -1;
	//camera.photo_type = YUV_TYPE;
	camera.win_width = ALIGN_16B(w);
	camera.win_height = ALIGN_16B(h);
	camera.fps = fps;
	//camera.photo_num = 5;
	camera.pixelformat = V4L2_PIX_FMT_NV21;
	//memcpy(camera.save_path, "/tmp", sizeof("/tmp"));
	//camera.ToRGB24 = NV21ToRGB24;
	camera.memory = V4L2_MEMORY_MMAP;//V4L2_MEMORY_MMAP V4L2_MEMORY_USERPTR V4L2_MEMORY_DMABUF

	/* 1.open /dev/videoX node */
	sprintf(camera_path, "%s%d", "/dev/video", camera.camera_index);
	LOGI("open %s!", camera_path);

	camera.videofd = open(camera_path, O_RDWR, 0);
	if (camera.videofd < 0) {
		LOGW(" open %s fail!!!", camera_path);

		return -1;
	}

	/* 2.Query device capabilities */
	memset(&cap, 0, sizeof(cap));
	if (ioctl(camera.videofd, VIDIOC_QUERYCAP, &cap) < 0) {
		LOGW("Query device capabilities fail!!!");
	} else {
		LOGV("Querey device capabilities succeed");
		LOGV("cap.driver=%s", cap.driver);
		LOGV("cap.card=%s", cap.card);
		LOGV("cap.bus_info=%s", cap.bus_info);
		LOGV("cap.version=0x%08x", cap.version);
		LOGV("cap.capabilities=0x%08x", cap.capabilities);
	}

	if ((cap.capabilities & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_CAPTURE_MPLANE)) <= 0) {
		LOGW("The device is not supports the Video Capture interface!!!");
		close(camera.videofd);
		return -1;
	}

	if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
		camera.driver_type = V4L2_CAP_VIDEO_CAPTURE_MPLANE;
		LOGV("LOGV V4L2_CAP_VIDEO_CAPTURE_MPLANE");
	} else if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
		camera.driver_type = V4L2_CAP_VIDEO_CAPTURE;
		LOGV("LOGV V4L2_CAP_VIDEO_CAPTURE");
	} else {
		LOGW(" %s is not a capture device.", camera_path);
		close(camera.videofd);
		return -1;
	}

	/* 3.select the current video input */
	memset(&inp, 0, sizeof(inp));
	inp.index = 0;
	inp.type = V4L2_INPUT_TYPE_CAMERA;
	if (ioctl(camera.videofd, VIDIOC_S_INPUT, &inp) < 0) {
		LOGW(" VIDIOC_S_INPUT failed! s_input: %d", inp.index);
		close(camera.videofd);
		return -1;
	}

#ifdef __USE_VIN_ISP__
	/* detect sensor type */
	camera.sensor_type = getSensorType(camera.videofd);
	if (camera.sensor_type == V4L2_SENSOR_TYPE_RAW) {
		camera.ispPort = CreateAWIspApi();
		LOGI("raw sensor use vin isp");
	}
#endif

	/* 7.set streaming parameters */
	memset(&parms, 0, sizeof(struct v4l2_streamparm));
	if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE)
		parms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	else
		parms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parms.parm.capture.timeperframe.numerator = 1;
	parms.parm.capture.timeperframe.denominator = camera.fps;
	if (ioctl(camera.videofd, VIDIOC_S_PARM, &parms) < 0) {
		LOGW(" Setting streaming parameters failed, numerator:%d denominator:%d",
			   parms.parm.capture.timeperframe.numerator,
			   parms.parm.capture.timeperframe.denominator);
		close(camera.videofd);
		return -1;
	}
	//ion_alloc_open();


	/* 8.get streaming parameters */
	memset(&parms, 0, sizeof(struct v4l2_streamparm));
	if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE)
		parms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	else
		parms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(camera.videofd, VIDIOC_G_PARM, &parms) < 0) {
		LOGW("Get streaming parameters failed!");
	} else {
		LOGI("Camera capture framerate is %u/%u",
			     parms.parm.capture.timeperframe.denominator, \
			     parms.parm.capture.timeperframe.numerator);
	}

	/* 9.set the data format */
	memset(&fmt, 0, sizeof(struct v4l2_format));
	if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		fmt.fmt.pix_mp.width = camera.win_width;
		fmt.fmt.pix_mp.height = camera.win_height;
		fmt.fmt.pix_mp.pixelformat = camera.pixelformat;
		fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
	} else {
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmt.fmt.pix.width = camera.win_width;
		fmt.fmt.pix.height = camera.win_height;
		fmt.fmt.pix.pixelformat = camera.pixelformat;
		fmt.fmt.pix.field = V4L2_FIELD_NONE;
	}

	if (ioctl(camera.videofd, VIDIOC_S_FMT, &fmt) < 0) {
		LOGW(" setting the data format failed!");
		close(camera.videofd);
		return -1;
	}

	if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
		if (camera.win_width != fmt.fmt.pix_mp.width || camera.win_height != fmt.fmt.pix_mp.height)
			LOGW(" does not support %u * %u", camera.win_width, camera.win_height);

		camera.win_width = fmt.fmt.pix_mp.width;
		camera.win_height = fmt.fmt.pix_mp.height;
		LOGI("VIDIOC_S_FMT succeed");
		LOGI("fmt.type = %d", fmt.type);
		LOGI("fmt.fmt.pix_mp.width = %d", fmt.fmt.pix_mp.width);
		LOGI("fmt.fmt.pix_mp.height = %d", fmt.fmt.pix_mp.height);
		LOGI("fmt.fmt.pix_mp.pixelformat = %s", get_format_name(fmt.fmt.pix_mp.pixelformat));
		LOGI("fmt.fmt.pix_mp.field = %d", fmt.fmt.pix_mp.field);

		if (ioctl(camera.videofd, VIDIOC_G_FMT, &fmt) < 0)
			LOGW(" get the data format failed!");

		camera.nplanes = fmt.fmt.pix_mp.num_planes;
	} else {
		if (camera.win_width != fmt.fmt.pix.width || camera.win_height != fmt.fmt.pix.height)
			LOGW(" does not support %u * %u", camera.win_width, camera.win_height);

		camera.win_width = fmt.fmt.pix.width;
		camera.win_height = fmt.fmt.pix.height;
		LOGI("VIDIOC_S_FMT succeed");
		LOGI("fmt.type = %d", fmt.type);
		LOGI("fmt.fmt.pix.width = %d", fmt.fmt.pix.width);
		LOGI("fmt.fmt.pix.height = %d", fmt.fmt.pix.height);
		LOGI("fmt.fmt.pix.pixelformat = %s", get_format_name(fmt.fmt.pix.pixelformat));
		LOGI("fmt.fmt.pix.field = %d", fmt.fmt.pix.field);
	}

	/* 10.Initiate Memory Mapping or User Pointer I/O */
	memset(&req, 0, sizeof(struct v4l2_requestbuffers));
	req.count = YUV_BUF_NUM_MAX;
	if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE)
		req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	else
		req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = camera.memory;
	if (ioctl(camera.videofd, VIDIOC_REQBUFS, &req) < 0) {
		LOGW(" VIDIOC_REQBUFS failed");
		close(camera.videofd);
		return -1;
	}

	/* Query the status of a buffers */
	camera.buf_count = req.count;
	LOGV("reqbuf number is %d", camera.buf_count);

	camera.buffers = (buffer *)calloc(req.count, sizeof(struct buffer));
	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		memset(&buf, 0, sizeof(struct v4l2_buffer));
		if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE)
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		else
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = camera.memory;
		buf.index = n_buffers;
		if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
			buf.length = camera.nplanes;
			buf.m.planes = (struct v4l2_plane *)calloc(buf.length, sizeof(struct v4l2_plane));
		}

		if (ioctl(camera.videofd, VIDIOC_QUERYBUF, &buf) == -1) {
			LOGW("VIDIOC_QUERYBUF error");

			if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE)
				free(buf.m.planes);
			free(camera.buffers);

			close(camera.videofd);

			return -1;
		}
		switch(camera.memory){
		case V4L2_MEMORY_MMAP:
			if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
				for (int i = 0; i < camera.nplanes; i++) {
					camera.buffers[n_buffers].length[i] = buf.m.planes[i].length;
					camera.buffers[n_buffers].start[i] = mmap(NULL , buf.m.planes[i].length,
								     PROT_READ | PROT_WRITE, \
								     MAP_SHARED , camera.videofd, \
								     buf.m.planes[i].m.mem_offset);

					LOGV(" map buffer index: %d, mem: %p, len: %x, offset: %x",
						n_buffers, camera.buffers[n_buffers].start[i], buf.m.planes[i].length,
							buf.m.planes[i].m.mem_offset);
				}
				free(buf.m.planes);
			}else {
					camera.buffers[n_buffers].length[0] = buf.length;
					camera.buffers[n_buffers].start[0] = mmap(NULL , buf.length,
							     PROT_READ | PROT_WRITE, \
							     MAP_SHARED , camera.videofd, \
							     buf.m.offset);
					LOGV(" map buffer index: %d, mem: %p, len: %x, offset: %x", \
					n_buffers, camera.buffers[n_buffers].start[0], buf.length, buf.m.offset);
			}
			break;
		case V4L2_MEMORY_USERPTR:
		case V4L2_MEMORY_DMABUF:
			/*if(camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE){
				for (int i = 0; i < camera.nplanes; i++) {
					camera.buffers[n_buffers].length[i] = buf.m.planes[i].length;
					camera.buffers[n_buffers].start[i] = (void *)ion_alloc(camera.buffers[n_buffers].length[i]);
					camera.buffers[n_buffers].fd[0] = ion_vir2fd(camera.buffers[n_buffers].start[i]);
				}
                        }else{
					camera.buffers[n_buffers].length[0] = buf.length;
					camera.buffers[n_buffers].start[0] = (void *)ion_alloc(camera.buffers[n_buffers].length[0]);
					camera.buffers[n_buffers].fd[0] = ion_vir2fd(camera.buffers[n_buffers].start[0]);

			}*/
			break;
		default:
			break;
		}
	}

	/* 11.Exchange a buffer with the driver */
	for (n_buffers = 0; n_buffers < req.count; n_buffers++) {
		memset(&buf, 0, sizeof(struct v4l2_buffer));
		if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE)
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		else
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = camera.memory;
		buf.index = n_buffers;
		if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
			buf.length = camera.nplanes;
			buf.m.planes = (struct v4l2_plane *)calloc(buf.length, sizeof(struct v4l2_plane));
		}
		switch(camera.memory){
			case V4L2_MEMORY_MMAP:
				break;
			case V4L2_MEMORY_USERPTR:
				if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE){
					for (int j = 0; j < camera.nplanes; j++){
						buf.m.planes[j].m.userptr = (unsigned long)camera.buffers[n_buffers].start[j];
						buf.m.planes[j].length = camera.buffers[n_buffers].length[j];
					}
				}else{
					buf.m.userptr = (unsigned long)camera.buffers[n_buffers].start[0];
					buf.length = camera.buffers[n_buffers].length[0];
				}
				break;
			case V4L2_MEMORY_DMABUF:
				/*if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE){
                                        for (int j = 0; j < camera.nplanes; j++){
						buf.m.planes[j].m.fd = ion_vir2fd(camera.buffers[n_buffers].start[j]);
                                        }
                                }else{
                                        buf.m.fd = ion_vir2fd(camera.buffers[n_buffers].start[0]);
                                }*/
                                break;
		}
		if (ioctl(camera.videofd, VIDIOC_QBUF, &buf) == -1) {
			LOGW(" VIDIOC_QBUF error");

			if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE)
				free(buf.m.planes);
			free(camera.buffers);

			close(camera.videofd);
			return -1;
		}
		if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE)
			free(buf.m.planes);
	}
	
	/* streamon */
	if (ioctl(camera.videofd, VIDIOC_STREAMON, &buf.type) == -1) {
		LOGW(" VIDIOC_STREAMON error! %s", strerror(errno));
		//ret = -1;
		return -1;
	} else
		LOGI("stream on succeed");

#ifdef __USE_VIN_ISP__
	/* setting ISP */
	if (camera.sensor_type == V4L2_SENSOR_TYPE_RAW
	    && !isOutputRawData(camera.pixelformat)) {
		camera.ispId = -1;
		camera.ispId = camera.ispPort->ispGetIspId(camera.camera_index);
		if (camera.ispId >= 0){
			LOGI("----ready to get ispExp and ispGain");
			camera.ispPort->ispStart(camera.ispId);
			camera.ispGain = camera.ispPort->ispGetIspGain(camera.ispId);
            camera.ispExp = camera.ispPort->ispGetIspExp(camera.ispId);
		}
	}
#endif
	memset(&read_buf, 0, sizeof(struct v4l2_buffer));
	if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE)
		read_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	else
		read_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	read_buf.memory = camera.memory;
	if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
		read_buf.length = camera.nplanes;
		read_buf.m.planes = (struct v4l2_plane *)calloc(camera.nplanes, sizeof(struct v4l2_plane));
	}
	LOGI("succ");
	
	return 0;
}

void YuvReader::Uninit(void)
{
	enum v4l2_buf_type type;
#if 1
	LOGI("start");
	//ion_alloc_close();
	/*
	LOGV("***************************************************************");
	LOGV(" Performance Testing---format:%s size:%u * %u", get_format_name(camera.pixelformat),
		   camera.win_width, camera.win_height);
	LOGV("***************************************************************");
	*/
	/* streamoff */
	if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE)
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	else
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(camera.videofd, VIDIOC_STREAMOFF, &type) == -1)
		LOGW(" VIDIOC_STREAMOFF error! %s", strerror(errno));

#ifdef __USE_VIN_ISP__
	/* stop ISP */
	if (camera.sensor_type == V4L2_SENSOR_TYPE_RAW) {
		if (camera.ispId >= 0 && !isOutputRawData(camera.pixelformat))
			camera.ispPort->ispStop(camera.ispId);

		DestroyAWIspApi(camera.ispPort);
		camera.ispPort = NULL;
	}
#endif
#endif
#if 1
	/* munmap camera.buffers */
	switch (camera.memory){
        case V4L2_MEMORY_MMAP:
            if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE){
                for (int i = 0; i < camera.buf_count; ++i) {
                    for (int j = 0; j < camera.nplanes; j++)
                        if (munmap(camera.buffers[i].start[j], camera.buffers[i].length[j])) {
                            LOGW("munmap error");
                        }
                    }
            }else{
                for (int i = 0; i < camera.buf_count; ++i) {
                    if (munmap(camera.buffers[i].start[0], camera.buffers[i].length[0])) {
                        LOGW("munmap error");
                    }
                }
            }
            break;
        case V4L2_MEMORY_USERPTR:
			break;
        case V4L2_MEMORY_DMABUF:
			/*if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE){
				for (int i = 0; i < camera.buf_count; ++i) {
					for (int j = 0; j < camera.nplanes; j++) {
						ion_free(camera.buffers[i].start[j]);
					}
				}
			}else{
				for (int i = 0; i < camera.buf_count; ++i)
					ion_free(camera.buffers[i].start[0]);
			*/
			break;
        default:
            break;
    }
	/* free camera.buffers and close camera.videofd */
	LOGI("close /dev/video%d", camera.camera_index);

	if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE)
		free(read_buf.m.planes);
	//释放内存
	free(camera.buffers);
	close(camera.videofd);
#endif
	LOGI("done");
}

int  YuvReader::OutBuf(uint8_t **y, uint8_t **c)
{
	int ret;
	
	/* dqbuf */
	ret = ioctl(camera.videofd, VIDIOC_DQBUF, &read_buf);
	if(ret < 0)
	{
		char *strerr = strerror(errno);
		if (errno == 11) {
			//busy
		} else {
			LOGW("ioctl VIDIOC_DQBUF fail ret %d %s", errno, strerr);
		}
		return -1;
	}
	if (ret == 0)
		LOGV("*****DQBUF[%d] FINISH*****", read_buf.index);

	switch (camera.memory){
		case V4L2_MEMORY_MMAP:
            break;
        case V4L2_MEMORY_USERPTR:
            if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE){
                for (int j = 0; j < camera.nplanes; j++){
                    read_buf.m.planes[j].m.userptr = (unsigned long)camera.buffers[read_buf.index].start[j];
                    read_buf.m.planes[j].length = camera.buffers[read_buf.index].length[j];
                }
            }else{
                read_buf.m.userptr = (unsigned long)camera.buffers[read_buf.index].start[0];
                read_buf.length = camera.buffers[read_buf.index].length[0];
            }
            break;
        case V4L2_MEMORY_DMABUF:
            /*if (camera.driver_type == V4L2_CAP_VIDEO_CAPTURE_MPLANE){
                for (int j = 0; j < camera.nplanes; j++){
                    read_buf.m.planes[j].m.fd = ion_vir2fd(camera.buffers[read_buf.index].start[j]);
                }
            } else{
                read_buf.m.fd = ion_vir2fd(camera.buffers[read_buf.index].start[0]);
            */
            break;
		default:
            break;
	}

	/* check the data buf for byte alignment */
	if (camera.pixelformat == V4L2_PIX_FMT_YUV422P || camera.pixelformat == V4L2_PIX_FMT_YUYV
	    || camera.pixelformat == V4L2_PIX_FMT_YVYU || camera.pixelformat == V4L2_PIX_FMT_UYVY
	    || camera.pixelformat == V4L2_PIX_FMT_VYUY) {
		if (camera.win_width * camera.win_height * 2 < read_buf.bytesused) {
			width16 = ALIGN_16B(camera.win_width);
			height16 = ALIGN_16B(camera.win_height);
		} else {
			width16 = camera.win_width;
			height16 = camera.win_height;
		}
	} else if (camera.pixelformat == V4L2_PIX_FMT_YUV420 || camera.pixelformat == V4L2_PIX_FMT_YVU420
		   || camera.pixelformat == V4L2_PIX_FMT_NV16 || camera.pixelformat == V4L2_PIX_FMT_NV61
		   || camera.pixelformat == V4L2_PIX_FMT_NV12 || camera.pixelformat == V4L2_PIX_FMT_NV21) {
		if (camera.win_width * camera.win_height * 1.5 < read_buf.bytesused) {
			width16 = ALIGN_16B(camera.win_width);
			height16 = ALIGN_16B(camera.win_height);
		} else {
			width16 = camera.win_width;
			height16 = camera.win_height;
		}
	} else {
		width16 = camera.win_width;
		height16 = camera.win_height;
	}
	
	if(read_buf.m.userptr == NULL)
	{
		LOGW("videodev_get_addr frame_buf[j].addrVirY == NULL");
		return -1;	
	}
	*y = (uint8_t *)camera.buffers[read_buf.index].start[0];
	*c = *y + (width16 * height16);
	uint8_t *tmp_y = *y;
	uint8_t *tmp_c = *c;
	//LOGV("y 0x%x 0x%x 0x%x 0x%x len %d", tmp_y[0], tmp_y[1], tmp_y[2], tmp_y[3], camera.buffers[read_buf.index].length[0]);
	//LOGV("c 0x%x 0x%x 0x%x 0x%x", tmp_c[0], tmp_c[1], tmp_c[2], tmp_c[3]);
	//LOGV("py 0x%p", tmp_y);
	
	return 0;
}

int YuvReader::InBuf(void)
{
	int ret;
	
	ret = ioctl(camera.videofd, VIDIOC_QBUF, &read_buf);
	if (ret < 0) {
		LOGW("ioctl VIDIOC_QBUF fail ret = %x", ret);
		
		return -1;
	}
	LOGV("************QBUF[%d] FINISH**************", read_buf.index);
	
	return 0;
}

int YuvReader::GetAddr(uint8_t **y, uint8_t **c)
{
	*y = (uint8_t *)camera.buffers[read_buf.index].start[0];
	*c = *y + (width16 * height16);
	
	return 0;
}

static char *get_format_name(unsigned int format)
{
	if (format == V4L2_PIX_FMT_YUV422P)
		return (char *)"YUV422P";
	else if (format == V4L2_PIX_FMT_YUV420)
		return (char *)"YUV420";
	else if (format == V4L2_PIX_FMT_YVU420)
		return (char *)"YVU420";
	else if (format == V4L2_PIX_FMT_NV16)
		return (char *)"NV16";
	else if (format == V4L2_PIX_FMT_NV12)
		return (char *)"NV12";
	else if (format == V4L2_PIX_FMT_NV61)
		return (char *)"NV61";
	else if (format == V4L2_PIX_FMT_NV21)
		return (char *)"NV21";
	else if (format == V4L2_PIX_FMT_HM12)
		return (char *)"MB YUV420";
	else if (format == V4L2_PIX_FMT_YUYV)
		return (char *)"YUYV";
	else if (format == V4L2_PIX_FMT_YVYU)
		return (char *)"YVYU";
	else if (format == V4L2_PIX_FMT_UYVY)
		return (char *)"UYVY";
	else if (format == V4L2_PIX_FMT_VYUY)
		return (char *)"VYUY";
	else if (format == V4L2_PIX_FMT_MJPEG)
		return (char *)"jpg";
	else if (format == V4L2_PIX_FMT_H264)
		return (char *)"H264";
	else if (format == V4L2_PIX_FMT_SBGGR8)
		return (char *)"BGGR8";
	else if (format == V4L2_PIX_FMT_SGBRG8)
		return (char *)"GBRG8";
	else if (format == V4L2_PIX_FMT_SGRBG8)
		return (char *)"GRBG8";
	else if (format == V4L2_PIX_FMT_SRGGB8)
		return (char *)"RGGB8";
	else if (format == V4L2_PIX_FMT_SBGGR10)
		return (char *)"BGGR10";
	else if (format == V4L2_PIX_FMT_SGBRG10)
		return (char *)"GBRG10";
	else if (format == V4L2_PIX_FMT_SGRBG10)
		return (char *)"GRBG10";
	else if (format == V4L2_PIX_FMT_SRGGB10)
		return (char *)"RGGB10";
	else if (format == V4L2_PIX_FMT_SBGGR12)
		return (char *)"BGGR12";
	else if (format == V4L2_PIX_FMT_SGBRG12)
		return (char *)"GBRG12";
	else if (format == V4L2_PIX_FMT_SGRBG12)
		return (char *)"GRBG12";
	else if (format == V4L2_PIX_FMT_SRGGB12)
		return (char *)"RGGB12";
	else
		return NULL;
}

static int isOutputRawData(int format)
{
	if (format == V4L2_PIX_FMT_SBGGR8 ||
	    format == V4L2_PIX_FMT_SGBRG8 ||
	    format == V4L2_PIX_FMT_SGRBG8 ||
	    format == V4L2_PIX_FMT_SRGGB8 ||

	    format == V4L2_PIX_FMT_SBGGR10 ||
	    format == V4L2_PIX_FMT_SGBRG10 ||
	    format == V4L2_PIX_FMT_SGRBG10 ||
	    format == V4L2_PIX_FMT_SRGGB10 ||

	    format == V4L2_PIX_FMT_SBGGR12 ||
	    format == V4L2_PIX_FMT_SGBRG12 ||
	    format == V4L2_PIX_FMT_SGRBG12 ||
	    format == V4L2_PIX_FMT_SRGGB12)
		return 1;
	else
		return 0;
}

#ifdef __USE_VIN_ISP__
static int getSensorType(int fd)
{
	int ret = -1;
	struct v4l2_control ctrl;
	struct v4l2_queryctrl qc_ctrl;

	if (fd == 0)
		return 0xFF000000;

	memset(&ctrl, 0, sizeof(struct v4l2_control));
	memset(&qc_ctrl, 0, sizeof(struct v4l2_queryctrl));
	ctrl.id = V4L2_CID_SENSOR_TYPE;
	qc_ctrl.id = V4L2_CID_SENSOR_TYPE;

	if (-1 == ioctl(fd, VIDIOC_QUERYCTRL, &qc_ctrl)) {
		LOGW(" query sensor type ctrl failed");
		return -1;
	}

	ret = ioctl(fd, VIDIOC_G_CTRL, &ctrl);
	if (ret < 0) {
		LOGW(" get sensor type failed,errno(%d)", errno);
		return ret;
	}

	return ctrl.value;
}
#endif

#ifdef __USE_VIN_ISP__
static int getSensorGain(int fd)
{
    int ret = -1;
    struct v4l2_control ctrl;

    if (fd == 0)
            return 0xFF000000;

    memset(&ctrl, 0, sizeof(struct v4l2_control));
    ctrl.id = V4L2_CID_GAIN;
    ret = ioctl(fd, VIDIOC_G_CTRL, &ctrl);
    if(ret < 0){
            return ret;
    }
    return ctrl.value;
}
#endif

#ifdef __USE_VIN_ISP__
static int getSensorExp(int fd)
{
        int ret = -1;
        struct v4l2_control ctrl;

        if (fd == 0)
                return 0xFF000000;

        memset(&ctrl, 0, sizeof(struct v4l2_control));
        ctrl.id = V4L2_CID_EXPOSURE;
        ret = ioctl(fd, VIDIOC_G_CTRL, &ctrl);
        if(ret < 0){
                return ret;
        }
        return ctrl.value;
}
#endif
