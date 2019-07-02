#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "can4linux.h"
#include "can.h"


typedef struct Data{
    unsigned char buf[256];
    unsigned char len;
}Data;



static int can0() {
  static int fd = 0;
  if(fd == 0) {
    	fd = open ("/dev/can0", O_RDWR | O_NONBLOCK);
  }
  return fd;
}

Err close_can0() {
  int fd = can0();
  return close(fd);
}


static Err read_string(int fd,int node, canmsg_t *rx, Data *data) {
    unsigned char len = rx->data[4];
    data->len = 0;
    canmsg_t msg;
    msg.flags = 0;
    msg.cob   = 0;
    msg.id    = (node & 0x7F) | 0x600;
    msg.data[0] =  0x60;

    while (len <= data->len)
    {

       	if(write (fd, &msg, 1)) return 3;
        canmsg_t nrx;
        if(read (fd, &nrx, 1)) return 2;
        int l = 7 - ((nrx.data[0] & 0xE)>>1);
        int i =1;
        for(i=1;i<=l;i++) {
            data->buf[data->len] = nrx.data[i];
            data->len++;
        }
        msg.data[0] ^= 0x10;
        if(l<7)break;
    }
    return 0;
}
static Err write_string(int fd,int node, Data *data) {
    unsigned char len = data->len;
    canmsg_t msg;
    canmsg_t nrx;

    msg.flags = 0;
	msg.cob   = 0;
    msg.length   = 8;
	msg.id    = (node & 0x7F) | 0x600;
    unsigned toggle = 0x0;
    unsigned pos = 0;
    while (len)
    {
        int l = len>7?7:len;
        msg.data [0] = toggle | (14-2*l) | (len<=7);
        int i = 1;
        for(i=1;i<=l;i++) {
            msg.data[i] = data->buf[pos];
            pos ++;
        }
       	if(write (fd, &msg, 1)) return 3;
        if(read (fd, &nrx, 1)) return 2;
        toggle ^= 0x10;
        len -=l;
    }
    return 0;
}



static Err set_value(canmsg_t *rx, Data *data) {
    if (0x40 != (rx->data[0]&0xE0))
		return 0;
	if (2 & rx->data[0]) {
		// 0: 4, 1: 3, 2: 2, 3: 1
		data->len   = 4 - (3 & (rx->data[0] >> 2));
		data->buf[0]  = rx -> data[4];

		if ( data->len > 1)
			 data->buf[1] = rx->data[5];

		if (data->len > 2)
			 data->buf[2] = rx->data[6];

		if (data->len > 3)
			 data->buf[3] = rx->data[7];
		// Es wurde eine Zahl der Länge '* size' mit dem Wert '* value' gelesen.
		return 0;
	}

	if (0x41 == rx->data[0]) {
		data->len  = rx -> data[4] | rx->data[5] << 8;
		return 0;
	}
	return 2;
}

// cmd = 0x20|19-(len<<2)
static Err can_msg (int fd, int node, int index, unsigned char sub, Data *data)
{
  static Data buffer;
  if(data == NULL) {
    buffer.len =0;
    memset(buffer.buf,0,256);
    data = &buffer;
  }
	canmsg_t msg;
    switch (data->len)
    {
    case 0:
        msg.data [0] = 0x40;
        break;
    case 1:
        msg.data [0] = 0x2F;
        msg.data[4]  = data->buf[0];
        break;
    case 2:
        msg.data [0] = 0x2B;
        msg.data[4]  = data->buf[0];
        msg.data[5]  = data->buf[1];
        break;
    case 3:
        msg.data [0] = 0x23;
        msg.data[4]  = data->buf[0];
        msg.data[5]  = data->buf[1];
        msg.data[6]  = data->buf[2];
        break;
    case 4:
        msg.data [0] = 0x23;
        msg.data[4]  = data->buf[0];
        msg.data[5]  = data->buf[1];
        msg.data[6]  = data->buf[2];
        msg.data[7]  = data->buf[3];
        break;
    default:
        msg.data [0] = 0x21;
        msg.data[4]  = data->len;
        break;
    }
	msg.flags = 0;
	msg.cob   = 0;
	msg.id    = (node & 0x7F) | 0x600;
	msg.length   = 8;
	msg.data[1] = (unsigned char) (index & 0xff);
	msg.data[2] =  (unsigned char)(index >> 8);
	msg.data[3] = (unsigned char) sub;
  canmsg_t rx;
	if (write (fd, &msg, 1))return 3;
  if(read (fd, &rx, 1)) return 2;
  switch (rx.data[0])
  {
    case 0x41: //read
        return read_string(fd,node,&rx,data);
    case 0x61:
        return write_string(fd,node,data);
    default:
        return set_value(&rx,data);
        break;
  }
  return 0;
}

static U32 value_u32(canmsg_t *rx) {
  unsigned value = 0;
  char len = 0;
  if (0x40 != (rx->data[0]&0xE0))
    return 0;
	if (2 & rx->data[0]) {
		// 0: 4, 1: 3, 2: 2, 3: 1
		len   = 4 - (3 & (rx->data[0] >> 2));
		value  = rx -> data[4];
  	if (len > 1) value |= rx->data[5] << 8;
		if (len > 2) value |= rx->data[6] << 16;
		if (len > 3) value |= rx->data[7] << 24;

		// Es wurde eine Zahl der Länge '* size' mit dem Wert '* value' gelesen.
		return value;
	}

	if (0x41 == rx ->data[0]) {
		value = rx -> data[4] | rx->data[5] << 8;
		// Es wurde die Länge einer Zeichenkette gelesen.
		return value;
	}
	return 0;
}

static U32 read_u32(int fd, int node,int index,U8 sub) {
    canmsg_t msg;
    msg.data[0] = 0x40;
    msg.flags   = 0;
    msg.cob     = 0;
    msg.id      = (node & 0x7F) | 0x600;
    msg.length  = 8;
    msg.data[1] = (unsigned char) (index & 0xff);
    msg.data[2] = (unsigned char)(index >> 8);
    msg.data[3] = (unsigned char) sub;
    if(write(can0(), &msg, 1))return 3;
    canmsg_t rx;
    if(read(can0(), &rx, 1)) return 2;
    return value_u32(&rx);
}

static Err write_u32(int fd, int node,int index,U8 sub,U8 len,U32 value) {
    canmsg_t msg;
    msg.data[0] = 0x40;
    msg.flags   = 0;
    msg.cob     = 0;
    msg.id      = (node & 0x7F) | 0x600;
    msg.length  = 8;
    msg.data[1] = (U8) (index & 0xff);
    msg.data[2] = (U8)(index >> 8);
    msg.data[3] = (U8) sub;
    msg.data[4] = (U8)            (unsigned)value;
    msg.data[5] = (U8) (1 < len ? (unsigned)value >>  8 : 0);
    msg.data[6] = (U8) (2 < len ? (unsigned)value >> 16 : 0);
    msg.data[7] = (U8) (3 < len ? (unsigned)value >> 24 : 0);
    if(write(fd, &msg, 1))return 3;
    canmsg_t rx;
    if(read(fd, &rx, 1)) return 2;
    return 0;
}
U32 read_unsigned(int node,int index,unsigned char sub) {
    return read_u32(can0(),node,index,sub);
}

static char* read_uart(int fd, int node,int index,U8 sub){
  static Data buffer;
  buffer.len =0;
  memset(buffer.buf,0,256);
  canmsg_t msg;
  msg.data [0] = 0x40;
  msg.flags = 0;
	msg.cob   = 0;
	msg.id    = (node & 0x7F) | 0x600;
	msg.length   = 8;
	msg.data[1] = (unsigned char) (index & 0xff);
	msg.data[2] =  (unsigned char)(index >> 8);
	msg.data[3] = (unsigned char) sub;
  canmsg_t rx;
	if (write (fd, &msg, 1))return NULL;
  if(read (fd, &rx, 1)) return NULL;
  read_string(fd,node,&rx,&buffer);
  return buffer.buf;
}
static Err write_uart(int fd,int node,int index,U8 sub,Data *buffer) {
  canmsg_t       msg;
  msg.data [0] = 0x21;
  msg.flags    = 0;
	msg.cob      = 0;
	msg.id       = (node & 0x7F) | 0x600;
	msg.length   = 8;
	msg.data[1]  = (unsigned char) (index & 0xff);
	msg.data[2]  = (unsigned char)(index >> 8);
	msg.data[3]  = (unsigned char) sub;
  msg.data[4]  = buffer->len;
	if (write (fd, &msg, 1))return 3;
  canmsg_t rx;
  if(read (fd, &rx, 1)) return 2;
  write_string(fd,node,buffer);
  return 0;
}
// Analog node

/// Analog inputs
U32 analog_get_in01(int node) {
  return 887;
}
U32 analog_get_in02(int node) {
  return 1587;
}
U32 analog_get_in03(int node) {
  return 1287;
}
U32 analog_get_in04(int node) {
  return 987;
}
U32 analog_get_in05(int node) {
  return 187;
}
// Analog out
U32 analog_get_out(int node) {
  return 87;
}
Err analog_set_out(int node,U32 value){
  return 0;
}
// Analog node temperatur
U32 analog_get_temp01(int node) {
  return 444;
}
U32 analog_get_temp02(int node) {
  return 364;
}
U32 analog_get_temp03(int node) {
  return 524;
}
char*    analog_get_uart01(int node){
  static Data buffer;
  buffer.len = 0;
  memset(buffer.buf,0,256);
  return "test uart01";
}
Err analog_set_baut01(int node,unsigned baut){

  return 0;
}

Err analog_set_uart01(int node,char *data){

  return 0;
}
char* analog_get_uart02(int node){
  static Data buffer;
  buffer.len = 0;
  memset(buffer.buf,0,256);
  return "analog 1 UART02";
}
Err analog_set_baut02(int node,unsigned baut){

}
Err analog_set_uart02(int node,char *data){

}

enum COMMAND {
  EXIT      = 0,
  READ_U32  = 1,
  READ_UART = 2,
  WRITE_UART= 3,
  WRITE_U32 = 4,
  TEST_READ_U32  = 10,
  TEST_READ_UART = 12,
  TEST_WRITE_UART= 13,
  TEST_WRITE_U32 = 14,
};

int main(int argc, char *argv[]) {

  char data[256];
  int i;
  int fd = can0();
  // if (fd == 0 ) {
  //   return 1;
  // }
  // printf("cmd: ");

  Data buffer;
  buffer.len = 0;
  memset(buffer.buf,0,256);
  while(1){
    int node,index;
    U8 cmd,len,sub;
    U32 value;
    fscanf(stdin,"%c:%i",&cmd,&node);
    switch(cmd){
      case EXIT:
        return 1;
      case READ_U32:
        fscanf(stdin,"%i:%c",&index,&sub);
        fprintf(stdout,"%u",read_u32(fd,node,index,sub));
        break;
      case READ_UART:
        fscanf(stdin,"%i:%c",&index,&sub);
        fprintf(stderr,"%s",read_uart(fd,node,index,sub));
        break;
      case WRITE_UART:
        fscanf(stdin,"%i:%c:'%s'",&index,&sub,buffer.buf);
        fprintf(stderr,"%i",write_uart(fd,node,index,sub,&buffer));
        break;
      case WRITE_U32:
        fscanf(stdin,"%i:%c:%c:%u",&index,&sub,&len,&value);
        fprintf(stderr,"%i",write_u32(fd,node,index,sub,len,value));
        break;
      case TEST_READ_U32:
        fscanf(stdin,"%i:%c",&index,&sub);
        fprintf(stdout,"READ_U32[cmd:%c node:%i index:%i sub:%c]",cmd,node,index,sub);
        break;
      case TEST_READ_UART:
        fscanf(stdin,"%i:%c",&index,&sub);
        fprintf(stdout,"READ_UART[cmd:%c node:%i index:%i sub:%c]",cmd,node,index,sub);
        break;
      case TEST_WRITE_UART:
        fscanf(stdin,"%i:%c:'%s'",&index,&sub,buffer.buf);
        fprintf(stdout,"WRITE_UART[cmd:%c node:%i index:%i sub:%c value:%s]",cmd,node,index,sub,buffer.buf);
        break;
      case TEST_WRITE_U32:
        fscanf(stdin,"%i:%c:%c:%u",&index,&sub,&len,&value);
        fprintf(stdout,"WRITE_U32[cmd:%c node:%i index:%i sub:%c len:%c value:%u]",cmd,node,index,sub,len,value);
        break;
      default:
        return 5;
        break;
    }
  }
  return 0;
}
