/********************************** (C) COPYRIGHT ******************************
* File Name          : DNS.c
* Author             : WCH
* Version            : V1.0
* Date               : 2018/12/05
* Description        : CH579 NET��DNSӦ����ʾ                  
*******************************************************************************/



/******************************************************************************/
/* ͷ�ļ����� */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "CH57x_common.h"
#include "core_cm0.h"
#include "CH57xNET.H"
#include "DNS.H"


#define CH57xNET_DBG                          1
/* �������� */
UINT8 DNS_SERVER_IP[4]={114,114,114,114};                                       /* DNS������ */
#define    IPPORT_DOMAIN    53                                                  /* DNSĬ�϶˿� */
UINT16 MSG_ID = 0x1100;                                                         /* ��ʶ */
UINT32 count = 0;
UINT8 dns_buf[MAX_DNS_BUF_SIZE];

/*******************************************************************************
* Function Name  : get16
* Description    : ��������UINT8����תΪUINT16��ʽ����
* Input          : s -UINT8��������
* Output         : None
* Return         : ת�����UINT16��������
*******************************************************************************/
UINT16 get16(UINT8 * s)
{
    UINT16 i;

    i = *s++ << 8;
    i = i + *s;
    return i;
}

/*******************************************************************************
* Function Name  : ParseName
* Description    : ��������������
* Input          : msg        -ָ���ĵ�ָ��
                   compressed -ָ��������������ָ��
                   buf        -������ָ�룬���ڴ��ת��������
* Output         : None
* Return         : ѹ�����ĵĳ���
*******************************************************************************/
int ParseName(UINT8 * msg, UINT8 * compressed, char * buf)
{
    UINT16 slen;                                                                /* ��ǰƬ�γ���*/
    UINT8 * cp;
    int clen = 0;                                                               /* ѹ���������� */
    int indirect = 0;
    int nseg = 0;                                                               /* �������ָ��Ƭ������ */

    cp = compressed;
    for (;;){
        slen = *cp++;                                                           /* ���ֽڵļ���ֵ*/
        if (!indirect) clen++;
        if ((slen & 0xc0) == 0xc0){                                             /* �����ֽڸ�������Ϊ1������ѹ����ʽ */
            if (!indirect) clen++;
            indirect = 1;
            cp = &msg[((slen & 0x3f)<<8) + *cp];                                /* �������ֽ���ֵָ��ƫ�Ƶ�ָ��λ�� */
            slen = *cp++;
        }
        if (slen == 0) break;                                                   /* ����Ϊ0������ */
        if (!indirect) clen += slen;
        while (slen-- != 0) *buf++ = (char)*cp++;
        *buf++ = '.';
        nseg++;
    }
    if (nseg == 0){
        /* ������; */
        *buf++ = '.';
    }
    *buf++ = '\0';
    return clen;                                                                /* ѹ�����ĳ��� */
}

/*******************************************************************************
* Function Name  : DnsQuestion
* Description    : ������Ӧ�����е������¼����
* Input          : msg  -ָ����Ӧ���ĵ�ָ��
                   cp   -ָ�������¼��ָ��
* Output         : None
* Return         : ָ����һ��¼��ָ��
*******************************************************************************/
UINT8 * DnsQuestion(UINT8 * msg, UINT8 * cp)
{
    int len;
    char name[MAX_DNS_BUF_SIZE];

    len = ParseName(msg, cp, name);
    cp += len;
    cp += 2;                                                                    /* ���� */
    cp += 2;                                                                    /* �� */
    return cp;
}

/*******************************************************************************
* Function Name  : DnsAnswer
* Description    : ������Ӧ�����еĻش��¼����
* Input          : msg  -ָ����Ӧ���ĵ�ָ��
                   cp   -ָ��ش��¼��ָ��
                   psip           
* Output         : None
* Return         :ָ����һ��¼��ָ��
*******************************************************************************/
UINT8 * DnsAnswer(UINT8 * msg, UINT8 * cp, UINT8 * pSip)
{
    int len, type;
    char name[MAX_DNS_BUF_SIZE];

    len = ParseName(msg, cp, name);
    cp += len;
    type = get16(cp);
    cp += 2;        /* ���� */
    cp += 2;        /* �� */
    cp += 4;        /* ����ʱ�� */
    cp += 2;        /* ��Դ���ݳ��� */
    switch ( type ){
        case TYPE_A:
            pSip[0] = *cp++;
            pSip[1] = *cp++;
            pSip[2] = *cp++;
            pSip[3] = *cp++;
            break;
        case TYPE_CNAME:
        case TYPE_MB:
        case TYPE_MG:
        case TYPE_MR:
        case TYPE_NS:
        case TYPE_PTR:
            len = ParseName(msg, cp, name);
            cp += len;
            break;
        case TYPE_HINFO:
        case TYPE_MX:
        case TYPE_SOA:
        case TYPE_TXT:
            break;
        default:
            break;
    }
    return cp;
}

/*******************************************************************************
* Function Name  : parseMSG
* Description    : ������Ӧ�����е���Դ��¼����
* Input          : msg  -ָ��DNS����ͷ����ָ��
                   cp   -ָ����Ӧ���ĵ�ָ��
* Output         : None
* Return         :�ɹ�����1�����򷵻�0
*******************************************************************************/

UINT8 parseMSG(struct dhdr * pdhdr, UINT8 * pbuf, UINT8 * pSip)
{
    UINT16 tmp;
    UINT16 i;
    UINT8 * msg;
    UINT8 * cp;

    msg = pbuf;
    memset(pdhdr, 0, sizeof(pdhdr));
    pdhdr->id = get16(&msg[0]);
    tmp = get16(&msg[2]);
    if (tmp & 0x8000) pdhdr->qr = 1;
    pdhdr->opcode = (tmp >> 11) & 0xf;
    if (tmp & 0x0400) pdhdr->aa = 1;
    if (tmp & 0x0200) pdhdr->tc = 1;
    if (tmp & 0x0100) pdhdr->rd = 1;
    if (tmp & 0x0080) pdhdr->ra = 1;
    pdhdr->rcode = tmp & 0xf;
    pdhdr->qdcount = get16(&msg[4]);
    pdhdr->ancount = get16(&msg[6]);
    pdhdr->nscount = get16(&msg[8]);
    pdhdr->arcount = get16(&msg[10]);
    /* �����ɱ����ݳ��Ȳ���*/
    cp = &msg[12];
    /* ��ѯ���� */
    for (i = 0; i < pdhdr->qdcount; i++)
    {
        cp = DnsQuestion(msg, cp);
    }
    /* �ش� */
    for (i = 0; i < pdhdr->ancount; i++)
    {
        cp = DnsAnswer(msg, cp, pSip);
    }
    /*��Ȩ */
    for (i = 0; i < pdhdr->nscount; i++)
    {
      /*������*/    ;
    }
    /* ������Ϣ */
    for (i = 0; i < pdhdr->arcount; i++)
    {
      /*������*/    ;
    }
    if(pdhdr->rcode == 0) return 1;                                             /* rcode = 0:�ɹ� */
    else return 0;
}

/*******************************************************************************
* Function Name  : put16
* Description    :UINT16 ��ʽ���ݰ�UINT8��ʽ�浽������
* Input          : s -�������׵�ַ
                   i -UINT16����
* Output         : None
* Return         : ƫ��ָ��
*******************************************************************************/
UINT8 * put16(UINT8 * s, UINT16 i)
{
    *s++ = i >> 8;
    *s++ = i;
    return s;
}

/*******************************************************************************
* Function Name  : MakeDnsQuery
* Description    : ����DNS��ѯ����
  input          : op   - �ݹ�
*                  name - ָ���������ָ��
*                  buf  - DNS������.
*                  len  - ��������󳤶�.
* Output         : None
* Return         : ָ��DNS����ָ��
*******************************************************************************/
UINT16 MakeDnsQueryMsg(UINT16 op, char * name, UINT8 * buf, UINT16 len)
{
    UINT8 *cp;
    char *cp1;
    char tmpname[MAX_DNS_BUF_SIZE];
    char *dname;
    UINT16 p;
    UINT16 dlen;

	//printf("Domain name:%s \n",name);
    cp = buf;
    MSG_ID++;
    cp = put16(cp, MSG_ID);                                                     /* ��ʶ */
    p = (op << 11) | 0x0100;            
    cp = put16(cp, p);                                                          /* 0x0100��Recursion desired */
    cp = put16(cp, 1);                                                          /* ��������1 */
    cp = put16(cp, 0);                                                          /* ��Դ��¼����0 */
    cp = put16(cp, 0);                                                          /* ��Դ��¼����0 */
    cp = put16(cp, 0);                                                          /* ������Դ��¼����0 */

    strcpy(tmpname, name);
    dname = tmpname;
    dlen = strlen(dname);
    for (;;){                                                                   /* ����DNS������������ʽ����URIд�뵽buf����ȥ */
        cp1 = strchr(dname, '.');
        if (cp1 != NULL) len = cp1 - dname;    
        else len = dlen;        
        *cp++ = len;            
        if (len == 0) break;
        strncpy((char *)cp, dname, len);
        cp += len;
        if (cp1 == NULL)
        {
            *cp++ = 0;        
            break;
        }
        dname += len+1;                                                         /* dname�׵�ַ���� */
        dlen -= len+1;                                                          /* dname���ȼ�С */
    }
    cp = put16(cp, 0x0001);                                                     /* type ��1------ip��ַ */
    cp = put16(cp, 0x0001);                                                     /* class��1-------��������ַ */
    return ((UINT16)(cp - buf));
}

/*******************************************************************************
* Function Name  : DnsQuery
* Description    : ����DNS��ѯ
  input          : s    -socket����
*                  name - ָ���������ָ��
                   pSip -��ѯ���
* Output         : None
* Return         : ��ѯ������ɹ�����1��ʧ�ܷ���-1
*******************************************************************************/
UINT8 DnsQuery(UINT8 s, UINT8 * name, UINT8 * pSip)
{
    struct dhdr dhp;

    UINT8 ret;
    UINT32 len;
    if(status >1 ){
        count++;
        DelayMs(10);
        if( count>20000 ){
#if CH57xNET_DBG
            printf("DNS Fail!!!!!\n");
#endif
            count=0;
            status = 0;
            return 2;
        } 
    }
    if(status == 1)
    {
        UDPSocketParamInit(s,DNS_SERVER_IP,4000,IPPORT_DOMAIN);
        status = 2;
#if CH57xNET_DBG
        printf(" 2 status = %d!\n",status);
#endif
    }
    if(status ==2)
    {
        len = MakeDnsQueryMsg(0,(char *)name, dns_buf, MAX_DNS_BUF_SIZE);
		ret = CH57xNET_SocketSend(s,dns_buf,&len);
        if ( ret ) return(0);
        else{
            status = 3;
#if CH57xNET_DBG
            printf("status = 3!\n");
#endif
        }
    }
    if(status ==4)
    {
        return(parseMSG(&dhp, dns_buf, pSip));                                  /*������Ӧ���Ĳ����ؽ��*/
		
    }
    return 0;
}

/*********************************** endfile **********************************/