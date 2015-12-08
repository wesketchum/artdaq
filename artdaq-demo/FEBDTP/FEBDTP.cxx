#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>


#include "TObject.h"

#include "FEBDTP.hxx"



ClassImp(FEBDTP)


int    FEBDTP::Init(const char * iface) 
{
 	tv.tv_sec = 0;  /* 0 Secs Timeout */
	tv.tv_usec = 500000;  // 500ms
        int sockfd;
	struct ifreq if_idx;
	struct ifreq if_mac;
        sprintf(ifName,"%s",iface);
        Init_FEBDTP_pkt();
 		/* Open RAW socket to send on */
	if ((sockfd_w = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) {
	    perror("sender: socket");
            return 0;
	}
	/* Open PF_PACKET socket, listening for EtherType ETHER_TYPE */
	if ((sockfd_r = socket(PF_PACKET, SOCK_RAW, gpkt.iptype)) == -1) {
		perror("listener: socket");	
		return 0;
	}

        if (setsockopt(sockfd_r, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval))== -1){
		perror("SO_SETTIMEOUT");
		return 0;
	}
        	/* Bind listener to device */
	if (setsockopt(sockfd_r, SOL_SOCKET, SO_BINDTODEVICE, ifName, IFNAMSIZ-1) == -1){
		perror("SO_BINDTODEVICE");
		return 0;
	}

	/* Get the index of the interface to send on */
	memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sockfd_w, SIOCGIFINDEX, &if_idx) < 0)
	    {perror("SIOCGIFINDEX"); return 0;}
	/* Get the MAC address of the interface to send on */
	memset(&if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sockfd_w, SIOCGIFHWADDR, &if_mac) < 0)
	    {perror("SIOCGIFHWADDR"); return 0;}
        memcpy(&srcmac,((uint8_t *)&if_mac.ifr_hwaddr.sa_data),6);
       Init_FEBDTP_pkt();
       return 1;
}
void FEBDTP::Init_FEBDTP_pkt(FEBDTP_PKT *pkt, UChar_t* src, UChar_t* dst)
{
	  memcpy(&(pkt->src_mac),src,6);
	  memcpy(&(pkt->dst_mac),dst,6);
	  pkt->iptype=0x0108; // IP type 0x0801
}

void FEBDTP::Init_FEBDTP_pkt(FEBDTP_PKT *pkt)
{
	  memcpy(&(pkt->src_mac),srcmac,6);
	  memcpy(&(pkt->dst_mac),dstmac,6);
	  pkt->iptype=0x0108; // IP type 0x0801
}

void FEBDTP::Init_FEBDTP_pkt()
{
	  memcpy(&(gpkt.src_mac),srcmac,6);
	  memcpy(&(gpkt.dst_mac),dstmac,6);
	  gpkt.iptype=0x0108; // IP type 0x0801
}


int FEBDTP::Send_pkt(FEBDTP_PKT *pkt, int len, int timeout_us)
{
	struct ifreq if_idx;
	struct ifreq if_mac;
	int numbytes;
	int retnumbytes;
        int numpacks=0;
        UChar_t   thisdstmac[6];
        memcpy(thisdstmac,pkt->dst_mac,6);
	struct sockaddr_ll socket_address;
        int jj;
	char str[32];
	/* Get the index of the interface to send on */
	memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sockfd_w, SIOCGIFINDEX, &if_idx) < 0)
	    {perror("SIOCGIFINDEX"); return 0;}
	/* Get the MAC address of the interface to send on */
	memset(&if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sockfd_w, SIOCGIFHWADDR, &if_mac) < 0)
	    {perror("SIOCGIFHWADDR");return 0;}
           // set receive timeout
       tv.tv_usec=timeout_us;
       if (setsockopt(sockfd_r, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval))== -1){
		perror("SO_SETTIMEOUT");
		return 0;
	}

	/* Index of the network device */
	socket_address.sll_ifindex = if_idx.ifr_ifindex;
	/* Address length*/
	socket_address.sll_halen = ETH_ALEN;
	/* Destination MAC */
	socket_address.sll_addr[0] = pkt->dst_mac[0];
	socket_address.sll_addr[1] = pkt->dst_mac[1];
	socket_address.sll_addr[2] = pkt->dst_mac[2];
	socket_address.sll_addr[3] = pkt->dst_mac[3];
	socket_address.sll_addr[4] = pkt->dst_mac[4];
	socket_address.sll_addr[5] = pkt->dst_mac[5];
	      CMD_stoa(gpkt.CMD,str);
	/* Send packet */
	 if (sendto(sockfd_w, (char*)pkt, len, 0, (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll)) < 0)
	    {if(Verbose>0) printf("***  Send CMD=%s failed!\n",str); return 0;}
	    else if(Verbose>0) printf(">> Packet with CMD=%s sent to %02x:%02x:%02x:%02x:%02x:%02x.\n",str,thisdstmac[0],thisdstmac[1],thisdstmac[2],thisdstmac[3],thisdstmac[4],thisdstmac[5]);

        numbytes=0;
        while(numbytes>=0)
        {
      //  memset(gpkt.Data,0,MAXPAYLOAD);	
	numbytes = recvfrom(sockfd_r, &gpkt, MAXPACKLEN, 0, NULL, NULL);
        if(numbytes<0 && numpacks==0) {if(Verbose>0) printf("No reply within %ld us!\n",tv.tv_usec); return 0;}
        if(numbytes<0) return retnumbytes;
        if(gpkt.src_mac[5] != thisdstmac[5] && thisdstmac[5] !=0xff) {if(Verbose>0) printf("****************  Packet received from wrong FEB: 0x%02x (must be 0x%02x) Skipping..\n",gpkt.src_mac[5], thisdstmac[5]); continue;} // NOT from our FEB
//	printf("<< listener: got packet %d bytes from  %02x:%02x:%02x:%02x:%02x:%02x.\n", numbytes,gpkt.src_mac[0],gpkt.src_mac[1],gpkt.src_mac[2],gpkt.src_mac[3],gpkt.src_mac[4],gpkt.src_mac[5]);
        
	if(gpkt.CMD==FEB_DATA_CDR || gpkt.CMD==FEB_DATA_FW) 	{
                 //received event data buffer
                // Print_gpkt_evts(numbytes);
		 if( fPacketHandler) fPacketHandler(numbytes); //Do something with received data
		}
        //else  
        if(Verbose>0)printf("<< "); if(Verbose>0) Print_gpkt(numbytes);
        //else return retnumbytes;
        retnumbytes=numbytes;
        numpacks++;
	}
        return retnumbytes;
}
int FEBDTP::SendCMD(UChar_t* mac, UShort_t cmd, UShort_t reg, UChar_t* buf)
{
        int numbytes=1;
        int retval=0;
        int packlen=64;
        int tout=50000;
        // set short timeout and Flush input buffer 
        tv.tv_usec=10;
       if (setsockopt(sockfd_r, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval))== -1){
		perror("SO_SETTIMEOUT");
		return 0;
	}      
	while( numbytes>0) {numbytes= recvfrom(sockfd_r, &gpkt, MAXPACKLEN, 0, NULL, NULL); } //flush input buffer
        Init_FEBDTP_pkt();
        memcpy(&gpkt.dst_mac,mac,6);        
        memcpy(&gpkt.CMD,&cmd,2);
        memcpy(&gpkt.REG,&reg,2);
        switch (cmd) {
                case FEB_GET_RATE :
 		case FEB_GEN_INIT : 
  		case FEB_GEN_HVON : 
  		case FEB_GEN_HVOF : 
 			tout=1000; //timeout 1ms
   			break;
  		case FEB_SET_RECV : 
    			memcpy(&gpkt.Data,buf,6); // Copy source mac to the data buffer
			gpkt.REG=VCXO;
			tout=500; //timeout 0.5ms
   			break;
  		case FEB_WR_SR : 
 			tout=50000; //timeout 50ms
    			memcpy(&gpkt.Data,buf,1); // Copy register value to the data buffer 
   			break;
  		case FEB_WR_SRFF : 
  		case FEB_WR_SCR  : 
  		case FEB_WR_PMR  : 
  		case FEB_WR_CDR : 
 			tout=50000; //timeout 50ms
     			memcpy(&gpkt.Data,buf,256); // Copy 256 register values to the data buffer
     			packlen=256+18; 
    			break;
  		case FEB_RD_CDR : 
 			tout=50000; //timeout 150ms
    			break;
   		case FEB_RD_FW : 
  		case FEB_WR_FW : 
     			memcpy(&gpkt.Data,buf,5); // Copy address and numblocks
 			tout=50000; //timeout 50ms
    			break;
 		case FEB_DATA_FW : 
     			memcpy(&gpkt.Data,buf,1024); // Copy 1kB  data buffer
			packlen=1024+18;
 			tout=50000; //timeout 50ms
    			break;

	  
	}
        packlen=Send_pkt(&gpkt, packlen,tout);
        if(packlen<=0) return 0; 
// analyse received packet in gpkt
        retval=0; //default - error
        switch (gpkt.CMD) {
  		case FEB_OK : 
			retval=1;
   			break;
   		case FEB_OK_SR : 
   		case FEB_OK_SCR : 
   		case FEB_OK_PMR : 
  		case FEB_OK_FW : 
     			memcpy(buf,&gpkt.Data,(packlen>256)?256:1); // Copy register value(s) from the data buffer 
                        retval=1;
   			break;
   		case FEB_DATA_CDR : 
   		case FEB_DATA_FW : 
 //   			memcpy(buf,&gpkt.Data,(packlen>MAXPAYLOAD)?MAXPAYLOAD:packlen); // Copy register value(s) from the data buffer 
                        retval=1;
   			break;
    		case FEB_ERR_SR : 
   		case FEB_ERR_SCR : 
   		case FEB_ERR_PMR : 
   		case FEB_ERR_CDR : 
   		case FEB_ERR_FW : 
    			printf("Error code in reply.\n");
   			break;
   		case FEB_EOF_CDR : 
   		case FEB_EOF_FW : 
                        retval=0;
                        break;
  		default : 
    			printf("Unrecognized code RECEIVED FROM CLIENT! packlen=%d\n",packlen);
			Print_gpkt(packlen);
        }
   
        return retval;
}


void FEBDTP::CMD_stoa(UShort_t cmd, char* str)
{
 switch ( cmd ) {

  case FEB_RD_SR : 
    sprintf(str,"FEB_RD_SR");
    break;
  case FEB_WR_SR : 
    sprintf(str,"FEB_WR_SR");
    break;
  case FEB_RD_SRFF : 
    sprintf(str,"FEB_RD_SRFF");
    break;
  case FEB_WR_SRFF : 
    sprintf(str,"FEB_WR_SRFF");
    break;
  case FEB_OK_SR : 
    sprintf(str,"FEB_OK_SR");
    break;
  case FEB_ERR_SR : 
    sprintf(str,"FEB_ERR_SR");
    break;

  case FEB_SET_RECV : 
    sprintf(str,"FEB_SET_RECV");
    break;
  case FEB_GEN_INIT : 
    sprintf(str,"FEB_GEN_INIT");
    break;
  case FEB_GET_RATE : 
    sprintf(str,"FEB_GET_RATE");
    break;
  case FEB_GEN_HVON : 
    sprintf(str,"FEB_GEN_HVON");
    break;
  case FEB_GEN_HVOF : 
    sprintf(str,"FEB_GEN_HVOF");
    break;
  case FEB_OK : 
    sprintf(str,"FEB_OK");
    break;
  case FEB_ERR : 
    sprintf(str,"FEB_ERR");
    break;

  case FEB_RD_SCR : 
    sprintf(str,"FEB_RD_SCR");
    break;
  case FEB_WR_SCR  : 
    sprintf(str,"FEB_WR_SCR");
    break;
  case FEB_OK_SCR : 
    sprintf(str,"FEB_OK_SCR");
    break;
  case FEB_ERR_SCR : 
    sprintf(str,"FEB_ERR_SCR");
    break;

  case FEB_RD_PMR : 
    sprintf(str,"FEB_RD_PMR");
    break;
  case FEB_WR_PMR  : 
    sprintf(str,"FEB_WR_PMR");
    break;
  case FEB_OK_PMR : 
    sprintf(str,"FEB_OK_PMR");
    break;
  case FEB_ERR_PMR : 
    sprintf(str,"FEB_ERR_PMR");
    break;

  case FEB_RD_CDR : 
    sprintf(str,"FEB_RD_CDR");
    break;
  case FEB_WR_CDR : 
    sprintf(str,"FEB_WR_CDR");
    break;
  case FEB_DATA_CDR : 
    sprintf(str,"FEB_DATA_CDR");
    break;
  case FEB_ERR_CDR : 
    sprintf(str,"FEB_ERR_CDR");
    break;
  case FEB_EOF_CDR : 
    sprintf(str,"FEB_EOF_CDR");
    break;

  case FEB_RD_FW : 
    sprintf(str,"FEB_RD_FW");
    break;
  case FEB_WR_FW : 
    sprintf(str,"FEB_WR_FW");
    break;
  case FEB_DATA_FW : 
    sprintf(str,"FEB_DATA_FW");
    break;
  case FEB_ERR_FW : 
    sprintf(str,"FEB_ERR_FW");
    break;
  case FEB_EOF_FW : 
    sprintf(str,"FEB_EOF_FW");
    break;
  case FEB_OK_FW : 
    sprintf(str,"FEB_OK_FW");
    break;

  default : 
    sprintf(str,"Unrecognized code");
 }
}

void FEBDTP::Print_gpkt(int truncat)
{
  int jj;
char str[32];
  printf("(%02x %02x %02x %02x %02x %02x) => ",gpkt.src_mac[0],gpkt.src_mac[1],gpkt.src_mac[2],gpkt.src_mac[3],gpkt.src_mac[4],gpkt.src_mac[5]);
  printf("(%02x %02x %02x %02x %02x %02x) ",gpkt.dst_mac[0],gpkt.dst_mac[1],gpkt.dst_mac[2],gpkt.dst_mac[3],gpkt.dst_mac[4],gpkt.dst_mac[5]);
  printf("IP: %04x ",gpkt.iptype);
  CMD_stoa(gpkt.CMD,str);
  printf("CMD: %04x (%s)",gpkt.CMD,str);
  printf(" REG: %04x\n",gpkt.REG);
/*  printf("Data: \n");
        for(jj=0;jj<truncat-18;jj++)
         {
         printf("%02x ",gpkt.Data[jj]);
         if((jj%32)==31) printf("\n");
        }
          printf("\n");   
*/     
}

void FEBDTP::Print_gpkt_evts(int truncat)
{
/*typedef struct {
	UChar_t flags; //flags defining event type, 1=T0 reset, 2=T1 reset or 4=scintillator trigger
	UInt_t T0;
	UInt_t T1;
	UShort_t adc[32]; //adc data on 32 channels
} Event_t;
*/

  int jj;
  int kk;
UInt_t T0,T1;
UShort_t adc;
char str[32];
  printf("(%02x %02x %02x %02x %02x %02x) => ",gpkt.src_mac[0],gpkt.src_mac[1],gpkt.src_mac[2],gpkt.src_mac[3],gpkt.src_mac[4],gpkt.src_mac[5]);
  printf("(%02x %02x %02x %02x %02x %02x) ",gpkt.dst_mac[0],gpkt.dst_mac[1],gpkt.dst_mac[2],gpkt.dst_mac[3],gpkt.dst_mac[4],gpkt.dst_mac[5]);
  printf("IP: %04x",gpkt.iptype);
  CMD_stoa(gpkt.CMD,str);
  printf("CMD: %04x (%s)",gpkt.CMD,str);
  printf(" Remaining events: %d\n",gpkt.REG);
  printf("Events: \n");
        jj=0;
        while(jj<truncat-18)
         {
//	printf("Flags: %02x ",gpkt.Data[jj]); jj++;
//	printf("%02x",gpkt.Data[jj]); jj++;
//	printf("%02x",gpkt.Data[jj]); jj++;
//	printf("%02x",gpkt.Data[jj]); jj++;
        
        printf("Flags: 0x%08x ",*(UInt_t*)(&gpkt.Data[jj]));jj=jj+4;
        T0=*(UInt_t*)(&gpkt.Data[jj]); jj=jj+4; 
//	printf("T0: %02x",gpkt.Data[jj]); jj++;
//	printf("%02x",gpkt.Data[jj]); jj++;
//	printf("%02x",gpkt.Data[jj]); jj++;
//	printf("%02x",gpkt.Data[jj]); jj++;
        T1=*(UInt_t*)(&gpkt.Data[jj]); jj=jj+4; 
	printf("T0=%u ns, T1=%u ns,  ",T0,T1);

//	printf(" T1: %02x",gpkt.Data[jj]); jj++;
//	printf("%02x",gpkt.Data[jj]); jj++;
//	printf("%02x",gpkt.Data[jj]); jj++;
//	printf("%02x",gpkt.Data[jj]); jj++;

	printf(" ADC[32]:\n"); 
   //     for(kk=0; kk<32; kk++){ printf("%02x ",gpkt.Data[jj]);jj++; } //if(jj>=(truncat-18)) return;}
  //      printf("\n");
  //      for(kk=0; kk<32; kk++){ printf("%02x ",gpkt.Data[jj]);jj++; } //if(jj>=(truncat-18)) return;}
        for(kk=0; kk<32; kk++)
		{
		adc=*(UShort_t*)(&gpkt.Data[jj]); jj++; jj++;  
		printf("%04u ",adc);		
		} //if(jj>=(truncat-18)) return;}
        printf("\n");
        }
          printf("\n");        
}

int FEBDTP::ScanClients() // Scan MAC addresses of FEBs within reach
{
 int num=0;
 for(num=0;num<255;num++)
 {
  memcpy(macs[nclients],dstmac,6);
  macs[nclients][5]=num;
  if(SendCMD(macs[nclients],FEB_SET_RECV,0,srcmac)>0)  { 
      for(int i=0;i<5;i++) printf("%02x:",macs[nclients][i]);
      printf("%02x ",macs[nclients][5]);
      printf("%s\n",gpkt.Data);  nclients++;} // client reply received
 }
 printf("\n%d clients found.\n",nclients);
 return nclients; 
}

int FEBDTP::ReadBitStream(const char * fname, UChar_t* buf) // read CITIROC SC bitstream into the buffer, buf[MAXPACKLEN]
{ 
  FILE *file = fopen(fname, "r");
       char line[128];
       char bits[128];
       char comment[128];
       char bit; 
       int ptr, byteptr;
       int bitlen=0;
       char ascii[MAXPACKLEN];
	while (fgets(line, sizeof(line), file)) {
	  bit=1; ptr=0; byteptr=0;
//	  	printf("%d: %s",bitlen,line);

	  while(bit!=0x27 && bit!=0 && ptr<sizeof(line) && bitlen<MAXPACKLEN) // ASCII(0x27)= '
	  {
	    bit=line[ptr];
	    ptr++;
	    if(bit==0x20 || bit==0x27) continue; //ignore spaces and apostrophe
	    if(Verbose) printf("%c",bit);
	    ascii[bitlen]=bit;
	    bitlen++;
	  }
//	printf("\n");
	}
	printf("FEBDTP::ReadBitStream: %d bits read from file %s.\n",bitlen,fname);
  fclose(file);
  memset(buf,0,MAXPACKLEN); //reset buffer
// Now encode ASCII bitstream into binary
  for(ptr=bitlen-1;ptr>=0;ptr--)
  {
    byteptr=(bitlen-ptr-1)/8;
    if(ascii[ptr]=='1')  buf[byteptr] |= (1 << (7-ptr%8)); 
 //   if((ptr%8)==0) printf("bitpos=%d buf[%d]=%02x\n",ptr,byteptr,buf[byteptr]);
  }
  return bitlen;
}

int FEBDTP::WriteBitStream(const char * fname, UChar_t* buf, int bitlen) // write CITIROC SC bitstream to file, buf[MAXPACKLEN]
{ 
  FILE *file = fopen(fname, "w");
  UChar_t byte;
  int ib=0;
  char ascii[MAXPACKLEN];
  for(int i=bitlen/8-1;i>=0;i--)
  {
   byte=buf[i];
   for(int j=0;j<8;j++) {if(byte & 0x80) ascii[ib]='1'; else ascii[ib]='0'; byte = byte<<1; ib++;}
  }
  for(int i=0;i<bitlen;i++) fprintf(file,"%c",ascii[i]); 
  fprintf(file,"\n"); 
  fclose(file);
  return bitlen;
}

int FEBDTP::WriteBitStreamAnnotated(const char * fname, UChar_t* buf, int bitlen) // write CITIROC SC bitstream to file, buf[MAXPACKLEN]
{ 
  FILE *file = fopen(fname, "w");
  UChar_t byte;
  int ib=0;
  char ascii[MAXPACKLEN];
  for(int i=bitlen/8-1;i>=0;i--)
  {
   byte=buf[i];
   for(int j=0;j<8;j++) {if(byte & 0x80) ascii[ib]='1'; else ascii[ib]='0'; byte = byte<<1; ib++;}
  }
  //for(int i=0;i<bitlen;i++) fprintf(file,"%c",ascii[i]);
  ib=0;
  for(int Ch=0; Ch<32;Ch++) {
	fprintf(file,"%c%c%c%c \' Ch%d 4-bit DAC_t ([0..3])\'\n",ascii[ib],ascii[ib+1],ascii[ib+2],ascii[ib+3],Ch);
        ib+=4;
  }
  for(int Ch=0; Ch<32;Ch++) {
	fprintf(file,"%c%c%c%c \' Ch%d 4-bit DAC ([0..3])\'\n",ascii[ib],ascii[ib+1],ascii[ib+2],ascii[ib+3],Ch);
        ib+=4;
  }
  fprintf(file,"%c \'Enable discriminator\'\n",ascii[ib++]);
  fprintf(file,"%c \' Disable trigger discriminator power pulsing mode (force ON)\'\n",ascii[ib++]);
  fprintf(file,"%c \' Select latched (RS : 1) or direct output (trigger : 0)\'\n",ascii[ib++]);
  fprintf(file,"%c \' Enable Discriminator Two\'\n",ascii[ib++]);
  fprintf(file,"%c \' Disable trigger discriminator power pulsing mode (force ON)\'\n",ascii[ib++]);
  fprintf(file,"%c \' EN_4b_dac\'\n",ascii[ib++]);
  fprintf(file,"%c \'PP: 4b_dac\'\n",ascii[ib++]);
  fprintf(file,"%c \' EN_4b_dac_t\'\n",ascii[ib++]);
  fprintf(file,"%c \'PP: 4b_dac_t\'\n",ascii[ib++]);
  for(int Ch=0; Ch<32;Ch++) fprintf(file,"%c",ascii[ib++]);
     fprintf(file," \' Allows to Mask Discriminator (channel 0 to 31) [active low]\'\n");
fprintf(file,"%c \' Disable High Gain Track & Hold power pulsing mode (force ON)\'\n",ascii[ib++]);
fprintf(file,"%c \' Enable High Gain Track & Hold\'\n",ascii[ib++]);
fprintf(file,"%c \' Disable Low Gain Track & Hold power pulsing mode (force ON)\'\n",ascii[ib++]);
fprintf(file,"%c \' Enable Low Gain Track & Hold\'\n",ascii[ib++]);
fprintf(file,"%c \' SCA bias ( 1 = weak bias, 0 = high bias 5MHz ReadOut Speed)\'\n",ascii[ib++]);
fprintf(file,"%c \'PP: HG Pdet\'\n",ascii[ib++]);
fprintf(file,"%c \' EN_HG_Pdet\'\n",ascii[ib++]);
fprintf(file,"%c \'PP: LG Pdet\'\n",ascii[ib++]);
fprintf(file,"%c \' EN_LG_Pdet\'\n",ascii[ib++]);
fprintf(file,"%c \' Sel SCA or PeakD HG\'\n",ascii[ib++]);
fprintf(file,"%c \' Sel SCA or PeakD LG\'\n",ascii[ib++]);
fprintf(file,"%c \' Bypass Peak Sensing Cell\'\n",ascii[ib++]);
fprintf(file,"%c \' Sel Trig Ext PSC\'\n",ascii[ib++]);
fprintf(file,"%c \' Disable fast shaper follower power pulsing mode (force ON)\'\n",ascii[ib++]);
fprintf(file,"%c \' Enable fast shaper\'\n",ascii[ib++]);
fprintf(file,"%c \' Disable fast shaper power pulsing mode (force ON)\'\n",ascii[ib++]);
fprintf(file,"%c \' Disable low gain slow shaper power pulsing mode (force ON)\'\n",ascii[ib++]);
fprintf(file,"%c \' Enable Low Gain Slow Shaper\'\n",ascii[ib++]);

fprintf(file,"%c%c%c \' Low gain shaper time constant commands (0…2)  [active low] 100\'\n",ascii[ib],ascii[ib+1],ascii[ib+2]);
        ib+=3;

fprintf(file,"%c \' Disable high gain slow shaper power pulsing mode (force ON)\'\n",ascii[ib++]);
fprintf(file,"%c \' Enable high gain Slow Shaper\'\n",ascii[ib++]);

fprintf(file,"%c%c%c \' High gain shaper time constant commands (0…2)  [active low] 100\'\n",ascii[ib],ascii[ib+1],ascii[ib+2]);
        ib+=3;

fprintf(file,"%c \' Low Gain PreAmp bias ( 1 = weak bias, 0 = normal bias)\'\n",ascii[ib++]);
fprintf(file,"%c \' Disable High Gain preamp power pulsing mode (force ON)\'\n",ascii[ib++]);
fprintf(file,"%c \' Enable High Gain preamp\'\n",ascii[ib++]);
fprintf(file,"%c \' Disable Low Gain preamp power pulsing mode (force ON)\'\n",ascii[ib++]);
fprintf(file,"%c \' Enable Low Gain preamp\'\n",ascii[ib++]);
fprintf(file,"%c \' Select LG PA to send to Fast Shaper\'\n",ascii[ib++]);
fprintf(file,"%c \' Enable 32 input 8-bit DACs\'\n",ascii[ib++]);
fprintf(file,"%c \' 8-bit input DAC Voltage Reference (1 = external 4,5V , 0 = internal 2,5V)\'\n",ascii[ib++]);

  for(int Ch=0; Ch<32;Ch++) {
	fprintf(file,"%c%c%c%c",ascii[ib],ascii[ib+1],ascii[ib+2],ascii[ib+3]); ib+=4;
	fprintf(file,"%c%c%c%c",ascii[ib],ascii[ib+1],ascii[ib+2],ascii[ib+3]); ib+=4;
	fprintf(file," %c \' Input 8-bit DAC Data channel %d – (DAC7…DAC0 + DAC ON), higher-higher bias\'\n",ascii[ib++],Ch);
  }

  for(int Ch=0; Ch<32;Ch++) {
	fprintf(file,"%c%c%c",ascii[ib],ascii[ib+1],ascii[ib+2]); ib+=3;
	fprintf(file,"%c%c%c ",ascii[ib],ascii[ib+1],ascii[ib+2]); ib+=3;
	fprintf(file,"%c%c%c",ascii[ib],ascii[ib+1],ascii[ib+2]); ib+=3;
	fprintf(file,"%c%c%c ",ascii[ib],ascii[ib+1],ascii[ib+2]); ib+=3;
	fprintf(file,"%c%c%c ",ascii[ib],ascii[ib+1],ascii[ib+2]); ib+=3;
	fprintf(file,"\' Ch%d   PreAmp config (HG gain[5..0], LG gain [5..0], CtestHG, CtestLG, PA disabled)\'\n",Ch);
  }


fprintf(file,"%c \' Disable Temperature Sensor power pulsing mode (force ON)\'\n",ascii[ib++]);
fprintf(file,"%c \' Enable Temperature Sensor\'\n",ascii[ib++]);
fprintf(file,"%c \' Disable BandGap power pulsing mode (force ON)\'\n",ascii[ib++]);
fprintf(file,"%c \' Enable BandGap\'\n",ascii[ib++]);
fprintf(file,"%c \' Enable DAC1\'\n",ascii[ib++]);
fprintf(file,"%c \' Disable DAC1 power pulsing mode (force ON)\'\n",ascii[ib++]);
fprintf(file,"%c \' Enable DAC2\'\n",ascii[ib++]);
fprintf(file,"%c \' Disable DAC2 power pulsing mode (force ON)\'\n",ascii[ib++]);

	fprintf(file,"%c%c ",ascii[ib],ascii[ib+1]); ib+=2;
	fprintf(file,"%c%c%c%c ",ascii[ib],ascii[ib+1],ascii[ib+2],ascii[ib+3]); ib+=4;
	fprintf(file,"%c%c%c%c \' 10-bit DAC1 (MSB-LSB): 00 1100 0000 for 0.5 p.e.\'\n",ascii[ib],ascii[ib+1],ascii[ib+2],ascii[ib+3]); ib+=4;

	fprintf(file,"%c%c ",ascii[ib],ascii[ib+1]); ib+=2;
	fprintf(file,"%c%c%c%c ",ascii[ib],ascii[ib+1],ascii[ib+2],ascii[ib+3]); ib+=4;
	fprintf(file,"%c%c%c%c \' 10-bit DAC2 (MSB-LSB): 00 1100 0000 for 0.5 p.e.\'\n",ascii[ib],ascii[ib+1],ascii[ib+2],ascii[ib+3]); ib+=4;


fprintf(file,"%c \' Enable High Gain OTA'  -- start byte 2\n",ascii[ib++]);
fprintf(file,"%c \' Disable High Gain OTA power pulsing mode (force ON)\'\n",ascii[ib++]);
fprintf(file,"%c \' Enable Low Gain OTA\'\n",ascii[ib++]);
fprintf(file,"%c \' Disable Low Gain OTA power pulsing mode (force ON)\'\n",ascii[ib++]);
fprintf(file,"%c \' Enable Probe OTA\'\n",ascii[ib++]);
fprintf(file,"%c \' Disable Probe OTA power pulsing mode (force ON)\'\n",ascii[ib++]);
fprintf(file,"%c \' Otaq test bit\'\n",ascii[ib++]);
fprintf(file,"%c \' Enable Val_Evt receiver\'\n",ascii[ib++]);
fprintf(file,"%c \' Disable Val_Evt receiver power pulsing mode (force ON)\'\n",ascii[ib++]);
fprintf(file,"%c \' Enable Raz_Chn receiver\'\n",ascii[ib++]);
fprintf(file,"%c \' Disable Raz Chn receiver power pulsing mode (force ON)\'\n",ascii[ib++]);
fprintf(file,"%c \' Enable digital multiplexed output (hit mux out)\'\n",ascii[ib++]);
fprintf(file,"%c \' Enable digital OR32 output\'\n",ascii[ib++]);
fprintf(file,"%c \' Enable digital OR32 Open Collector output\'\n",ascii[ib++]);
fprintf(file,"%c \' Trigger Polarity\'\n",ascii[ib++]);
fprintf(file,"%c \' Enable digital OR32_T Open Collector output\'\n",ascii[ib++]);
fprintf(file,"%c \' Enable 32 channels triggers outputs\'\n",ascii[ib++]);
 // for(int i=ib;i<bitlen;i++) fprintf(file,"%c",ascii[i]);
  fprintf(file,"\n"); 
  fclose(file);
  return bitlen;
}

UChar_t SwapBits( UChar_t bt)
{
  int i=0;
  UChar_t retval=0;
  retval |= (bt & 0x01) << 7;
  retval |= (bt & 0x02) << 5;
  retval |= (bt & 0x04) << 3;
  retval |= (bt & 0x08) << 1;
  retval |= (bt & 0x10) >> 1;
  retval |= (bt & 0x20) >> 3;
  retval |= (bt & 0x40) >> 5;
  retval |= (bt & 0x80) >> 7;
  return retval;
}

void FEBDTP::WriteLVBitStream(const char * fname, UChar_t* buf, Bool_t rev) // write CITIROC SC bitstream from the buffer, buf[MAXPACKLEN], to LabView setup file
{
  int i=0;
    FILE *file = fopen(fname, "w");
 //   fprintf(file,"Normal:\n");  
    if(!rev)
   for(i=0;i<143;i++)
    {
      fprintf(file,"%02x",buf[i]);
    }
 //   fprintf(file,"\nReverse:\n");  
    else for(i=142;i>=0;i--)
    {
      fprintf(file,"%02x",SwapBits(buf[i]));
    }
  fprintf(file,"\n");  
  fclose(file);

}

void FEBDTP::ReadLVBitStream(const char * fname, UChar_t* buf, Bool_t rev) 
{
  int i=0;
  UChar_t bt;
     char line[1024];
     unsigned int ibt;
     FILE *file = fopen(fname, "r");
     fgets(line, sizeof(line), file); //one line only
    if(!rev)
      for(i=0;i<143;i++)     //Read normal ordered file
    {
      sscanf(&line[i*2],"%02x",&ibt);
      buf[i]=ibt;
    }
    else 
      for(i=0;i<143;i++)     //Read reverse ordered file
    {
      sscanf(&line[i*2],"%02x",&ibt);
      bt=ibt;
      buf[142-i]=SwapBits(bt);
    }

  fclose(file);

}
void FEBDTP::setPacketHandler( void (*fhandler)(int))
{
  fPacketHandler=fhandler;
}

void FEBDTP::PrintMacTable()
{
  printf("Clients table:\n");
  for(int feb=0; feb<nclients;feb++)
 {
  for(int i=0;i<5;i++) printf("%02x:",macs[feb][i]);
  printf("%02x ",macs[feb][5]);
  printf(" -  client %d\n",feb);
 }
}


