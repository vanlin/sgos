/*
 *		BXML Parser
 *	  Huang Guan (gdxxhg@gmail.com)
 *	2009年7月26日 星期日 Created.
 *	2009年8月9日 星期日 ported to SGOS2.
 *	20090823 mk_node()
 *	20090827 readname()
 *  
 */

#include <stdlib.h>
#include <string.h>
#include <bxml.h>

// it's no use in kernel mode, but keep it
static char _err_str[32];
#define MIN(a,b) (a>b?b:a)

static void seterr( struct BXML_DATA* xml, const char* str )
{
	strncpy( _err_str, str, 32-1 );
	xml->error = 1;
}


static void* safe_malloc( uint size )
{
	return malloc( size );
}

static void safe_free( void* p )
{
	free( p );
}

// it's no use in kernel mode, but keep it
const char* bxml_lasterr()
{
	return _err_str;
}

//创建一个节点
static struct BXML_NODE* create_node( struct BXML_DATA* xml )
{
	struct BXML_NODE* nod;
	nod = (struct BXML_NODE*) safe_malloc( sizeof(struct BXML_NODE) );
	if( !nod ){
		seterr( xml, "no memory to allocate node.");
		return NULL;
	}
	memset( (void*)nod, 0, sizeof(struct BXML_NODE) );
	if( xml && xml->node_root == NULL )
		xml->node_root = nod;
	return nod;
}

//创建一个子节点
static struct BXML_NODE* create_childnode( struct BXML_DATA* xml, struct BXML_NODE* cur )
{
	struct BXML_NODE* nod;
	nod = create_node( xml );
	if( !nod )
		return NULL;
	if( cur ) {
		nod->node_next = cur->node_child;
		//建立链表关系
		if( cur->node_child ){	//如果已经有儿子，插入到儿子前面
			cur->node_child->node_pre = nod;
		}
		nod->node_parent = cur;
		cur->node_child = nod;
	}
	return nod;
}

//创建一个属性节点
static struct BXML_NODE* create_attrnode( struct BXML_DATA* xml, struct BXML_NODE* cur )
{
	struct BXML_NODE* nod;
	nod = create_node( xml );
	if( !nod )
		return NULL;
	if( cur ) {
		//建立链表关系
		nod->node_next = cur->node_attr;
		if( cur->node_attr )	//如果已经有属性，插入到属性前面
			cur->node_attr->node_pre = nod;
		nod->node_parent = cur;
		cur->node_attr = nod;
	}
	return nod;
}

//创建一个相邻节点
static struct BXML_NODE* create_nextnode( struct BXML_DATA* xml, struct BXML_NODE* cur )
{
	struct BXML_NODE* nod;
	nod = create_node( xml );
	if( !nod )
		return NULL;
	if( cur ) {
		//建立链表关系
		nod->node_pre = cur;
		if( cur->node_next )	//已经有下一个，则插入
			cur->node_next->node_pre = nod;
		cur->node_next = nod;
		nod->node_parent = cur->node_parent;
	}
	return nod;
}

// XML语法分析，长过程
static void parse_xml_node( struct BXML_DATA* xml, const char* mem )
{
	enum PARSE_STATE{
		OutsideTag,
		TagName,
		TagEnd,
		Text,
		AttributeName,
		AttributeValue,
		Header,
		Comment
	};
	enum PARSE_STATE state;
	const char* p, *v;
	struct BXML_NODE* cur;
	cur = NULL;
	state = OutsideTag;
	for( p=mem; *p; p++ ){
		switch( state ){
		case OutsideTag:	//
			switch(*p){
			case '<':	//'<' a new node starts
				if( *(p+1)=='/' ){	//end tag
					cur = cur->node_parent;
					state = TagEnd;
				}else if( *(p+1)=='?' ){	//xml header
					state = Header;
				}else if( *(p+1)=='!' ){	//xml comment
					state = Comment;
				}else{
					state = TagName;
					cur = create_nextnode( xml, cur );
					if( !cur ) return;
				}
				v = p+1;
				break;
			case ' ':	case '\r':	case '\n':	case '\t':
				break;
			default:
				printf("## incorrect character: %c\n", *p );
				break;
			}
			break;
		case Header:
			if(*p=='>')
				state = OutsideTag;
			break;
		case Comment:
			if(*p=='>')
				state = OutsideTag;
			break;
		case TagName:	//<tag
			switch(*p){
			case '>':
			case ' ':	case '\r':	case '\n':	case '\t':	case '/':
				if( (int)(p-v)>0 ){	//Tag Name
					int len = (p-v);
					if( len > 0 ){
						cur->name_len = len;
						cur->name = (char*)safe_malloc( cur->name_len+1 );
						if( cur->name ){
							memcpy( cur->name, v, len );
							cur->name[cur->name_len] = '\0';
						}
					}
				}
				v = p+1;
				if( *p == '>' ){
					state = Text;
				}else{
					state = AttributeName;
				}
				break;
			}
			break;
		case Text:	//<tag>Text</tag>
			if( *p == '<' ){
				if( *(p+1) == '/' ){//end tag??
					int len = (p-v);
					if( len > 0 ){
						cur->value_len = len;
						cur->value = (char*)safe_malloc( cur->value_len+1 );
						if( cur->value ){
							memcpy( cur->value, v, len );
							cur->value[cur->value_len] = '\0';
						}
					}
					state = TagEnd;
				}else{	//child tag
					state = TagName;
					cur= create_childnode( xml, cur );
					if( !cur ) return;
				}
				v = p+1;
			}
			break;
		case TagEnd:	//</tag>
			if( *p == '>' ){
				if( *v == '/' && strncmp( v+1, cur->name, MIN(cur->name_len, (int)(p-v-1)) )==0 ){
					state = OutsideTag;
				}else{
					printf("## incorrect end tag for <%s>\n", cur->name );
				}
//				kprintf("node name:%s  value:%s\n", cur->name, cur->value );
			}
			break;
		case AttributeName:	//name=""
			switch(*p){
			case ' ':	case '\r':	case '\n':	case '\t':	case '=':
				if( (int)(p-v)>1 ){ //a name is ok
					int len = (p-v);
					cur = create_attrnode( xml, cur );
					if( !cur ) return;
					if( len > 0 ){
						cur->name_len = len;
						cur->name = (char*)safe_malloc( cur->name_len+1 );
						if( cur->name ){
							memcpy( cur->name, v, len );
							cur->name[cur->name_len] = '\0';
						}
					}
				}
				v = p+1;
				if( *p == '=' ){
					state = AttributeValue;
				}
				break;
			case '>':
				state = Text;
				v = p+1;
				break;
			case '/':	//short end tag
				if( *(++p) == '>' ){
					state = OutsideTag;
				}else{
					printf("## no > after /\n" );
				}
				break;
			}
			break;
		case AttributeValue:
			if( *p=='"' ){
				if( *(v-1)=='"' ){ //a value is ok
					int len = (p-v);
					if( len > 0 ){
						cur->value_len = len;
						cur->value = (char*)safe_malloc( cur->value_len+1 );
						if( cur->value ){
							memcpy( cur->value, v, len );
							cur->value[cur->value_len] = '\0';
						}
					}
					state = AttributeName;
//					kprintf("attr name:%s  value:%s\n", cur->name, cur->value );
					//attribute is ok.
					cur = cur->node_parent;
				}
				v = p+1;
			}
			break;
		}
	}
}

//解析BXML节点
static const void* parse_bxml_node( struct BXML_DATA* xml, const void* mem, int type )
{
	struct BXML_NODE* cur;
	unsigned short	child_count;
	unsigned char	attr_count;
	int i;
	const void* p;
	p = (void*)mem;
	//读一个节点
	if( type == BTYPE_ATTRIBUTE ){
		xml->node_cur = cur = create_attrnode( xml, xml->node_cur ); 
	}else if( type == BTYPE_NODE ){
		xml->node_cur = cur = create_childnode( xml, xml->node_cur ); 
	}
	if( !cur )
		return NULL;
	cur->name_len = *(unsigned char*)p;		//节点名长度
	p++;
	cur->name = (char*)safe_malloc( cur->name_len+1 );
	if( cur->name ){
		memcpy( cur->name, p, cur->name_len );	//节点名
		cur->name[cur->name_len] = '\0';
	}
	p+=cur->name_len;
	attr_count = *(unsigned char*)p;		//属性个数
	p++;
	child_count = *(unsigned short*)p;		//子节点个数
	p+=sizeof(unsigned short);
	cur->value_len = *(unsigned int*)p;		//值长度
	p+=sizeof(unsigned int);
	if( cur->value_len > 0 ){
		cur->value = (char*)safe_malloc( cur->value_len+1 );
		if( cur->value ){
			memcpy( cur->value, p, cur->value_len );	//值
			cur->value[cur->value_len] = '\0';
		}
		p+=cur->value_len;
	}
	//读下一个节点
	for( i=0; i<attr_count&& p; i++ ){
		p = parse_bxml_node( xml, p, BTYPE_ATTRIBUTE );
		if( xml->node_cur )
			xml->node_cur = xml->node_cur->node_parent;
	}
	for( i=0; i<child_count&& p; i++ ){
		p = parse_bxml_node( xml, p, BTYPE_NODE );
		if( xml->node_cur )
			xml->node_cur = xml->node_cur->node_parent;
	}
	return p;
}

//load xml data from memory buffer
struct BXML_DATA* bxml_parse( const char* mem )
{
	struct BXML_DATA* xml;
	xml = (struct BXML_DATA*) safe_malloc( sizeof(struct BXML_DATA) );
	if( !xml ){
		seterr( xml, "no memory to allocate for xml");
		return NULL;
	}
	memset( (void*)xml, 0, sizeof(struct BXML_DATA) );
	if( *((unsigned int*)mem) == BXML_MAGIC ){
		struct BXML_HEADER* bh;
		bh = (struct BXML_HEADER*)mem;
		xml->version = bh->bxml_version;
		xml->flag = bh->bxml_flag;
		parse_bxml_node( xml, (const void*)(mem+sizeof(struct BXML_HEADER)), BTYPE_NODE );
	}else{
		parse_xml_node( xml, mem );
	}
	xml->node_cur = xml->node_root;
	return xml;
}


static int build_xml_node( struct BXML_NODE* nod, int level, char* mem, int mem_size )
{
	struct BXML_NODE* p;
	int i, len, ret;
	len = mem_size;
	len -= strlen(nod->name)+1+level*1;
	//tag name
	if( len>0 ){
		for( i=0;i<level;i++ )
			strcat( mem, "\t" );
		strcat( mem, "<" );
		strcat( mem, nod->name );
	}
	//attributes
	for( p=nod->node_attr; p; p=p->node_next ){
		len -= strlen(p->name)+p->value_len+4;
		if( len>0 )
			sprintf( mem, "%s %s=\"%s\"", mem, p->name, p->value );
	}
	//body
	if( nod->node_child ){
		len -= 2;
		if(len>0) strcat( mem, ">\n" );
		len = build_xml_node( nod->node_child, level+1, mem+strlen(mem), len );
		len -= strlen(nod->name)+4+level*1;
		if( len>0 ){
			for( i=0;i<level;i++ )
				strcat( mem, "\t" );
			sprintf( mem, "%s</%s>\n", mem, nod->name );
		}
	}else if( !nod->node_attr || nod->value_len>0 ){
		len -= 1;
		if(len>0) strcat( mem, ">" );
		len -= nod->value_len;
		if( len>0 && nod->value )
			strcat( mem, nod->value );
		len -= strlen(nod->name)+4;
		if( len>0 )
			sprintf( mem, "%s</%s>\n", mem, nod->name );
	}else{ //short tag
		len -=4;
		if( len>0 )
			strcat( mem, " />\n" );
	}
	if( nod->node_next ){
		len = build_xml_node( nod->node_next, level, mem+strlen(mem), len );
	}
	return len;
}

//build xml into memory
int bxml_buildxml( struct BXML_DATA* xml, char* mem, int mem_size )	
{
	int len;
	memset( mem, 0, mem_size );
	//build xml body
	len = build_xml_node( xml->node_root, 0, mem, mem_size );
	return (mem_size-len);
}

//calc the size of a node(including its children & attribute nodes )
static size_t calc_node_size( struct BXML_NODE* nod )
{
	struct BXML_NODE* p;
	size_t len = 0;
	len = 8 + nod->name_len + nod->value_len;
	for( p=nod->node_attr;p;p=p->node_next)
		len += calc_node_size( p );
	for( p=nod->node_child;p;p=p->node_next )
		len += calc_node_size( p );
	return len;
}

size_t bxml_buffer_size( struct BXML_DATA* xml )
{
	return sizeof(struct BXML_HEADER)+calc_node_size( xml->node_root );
}

static int build_bxml_node( struct BXML_NODE* nod, char* mem, int mem_size )
{
	struct BXML_NODE* p;
	char* q;
	int child_count;
	int attr_count;
	int i, len, ret;
	q = mem;
	len = mem_size - 8 - nod->name_len - nod->value_len;
	if( len < 0 ){
		return -BXML_BUFF_TOO_SMALL;
	}
	//count
	for( p=nod->node_attr, attr_count=0;p && attr_count<=255;p=p->node_next )
		attr_count ++;
	for( p=nod->node_child, child_count=0;p && child_count<=65535;p=p->node_next )
		child_count ++;
	//write node data
	//node name
	memcpy( q, &nod->name_len, sizeof(nod->name_len) );	
	q += sizeof(nod->name_len);
	memcpy( q, nod->name, nod->name_len );
	q += nod->name_len;
	memcpy( q, &attr_count, sizeof(unsigned char) );	
	q += sizeof(unsigned char);
	memcpy( q, &child_count, sizeof(unsigned short) );	
	q += sizeof(unsigned short);
	memcpy( q, &nod->value_len, sizeof(nod->value_len) );	
	q += sizeof(nod->value_len);
	memcpy( q, nod->value, nod->value_len );
	q += nod->value_len;
	//build attr
	for( p=nod->node_attr, attr_count=0;p && attr_count<=255;p=p->node_next, attr_count++){
		ret = build_bxml_node( p, q, len );
		if( ret<0 )
			return ret;
		q += (len-ret);
		len = ret;
	}
	for( p=nod->node_child, child_count=0;p && child_count<=65535;p=p->node_next, child_count++ ){
		ret = build_bxml_node( p, q, len );
		if( ret<0 )
			return ret;
		q += (len-ret);
		len = ret;
	}
	return len;
}

//build bxml into memory
int bxml_build( struct BXML_DATA* xml, char* mem, int mem_size )	
{
	int len;
	memset( mem, 0, mem_size );
	if( mem_size < sizeof(struct BXML_HEADER) )
		return -BXML_BUFF_TOO_SMALL;
	len = mem_size - sizeof(struct BXML_HEADER);
	//header
	struct BXML_HEADER* bh;
	bh = (struct BXML_HEADER*)mem;
	bh->bxml_magic = BXML_MAGIC;
	bh->bxml_version = xml->version;
	bh->bxml_flag = xml->flag;
	//build bxml body
	len = build_bxml_node( xml->node_root, mem+sizeof(struct BXML_HEADER), len );
	if( len < 0 )
		return len;
	return (mem_size-len);
}

//free all nodes recursively
static void free_node( struct BXML_DATA* xml, struct BXML_NODE* nod )
{
	while( nod ){
		if( nod->node_attr )
			free_node( xml, nod->node_attr );
		if( nod->node_child )
			free_node( xml, nod->node_child );
		if( nod->value )
			safe_free( nod->value );
		if( nod->node_next ){
			nod = nod->node_next;
			safe_free( nod->node_pre );
		}else{
			safe_free( nod );
			nod = NULL;
		}
	}
}

//free xml data
void bxml_free( struct BXML_DATA* xml )
{
	free_node( xml, xml->node_root );
	safe_free( xml );
}

static struct BXML_NODE* get_attrnode( struct BXML_DATA* xml, struct BXML_NODE* parent, 
	const char* name, int create )
{
	struct BXML_NODE* nod;
	nod = parent->node_attr;
	while( nod ){
		if( strcmp( nod->name, name )==0 )
			return nod;
		nod = nod->node_next;
	}
	//not found.
	if( create ){
		nod = create_attrnode( xml, parent );
		nod->name_len = strlen( name );
		nod->name = (char*)safe_malloc( nod->name_len+1 );
		if( nod->name ){
			memcpy( nod->name, name, nod->name_len );
			nod->name[nod->name_len] = '\0';
		}
		return nod;
	}else{
		return NULL;
	}
}

static struct BXML_NODE* get_childnode( struct BXML_DATA* xml, struct BXML_NODE* parent, 
	char* name, char* condition, int create )
{
	struct BXML_NODE* nod;
	nod = parent->node_child;
	if( !condition ){
		//specail one
		if( strcmp(name, "..." )==0 ){
			if( nod )
				return nod;
			else
				return parent;
		}else if( strcmp( name, ".." )==0 ){
			if( parent->node_parent )
				return parent->node_parent;
			else
				return parent;
		}else if( strcmp( name, "." )==0 ){
			return parent;
		}
		//search nodes
		while( nod ){
			if( strcmp( nod->name, name )==0 )
				return nod;
			nod = nod->node_next;
		}
		//not found.
		if( create ){
			nod = create_childnode( xml, parent );
			nod->name_len = strlen( name );
			nod->name = (char*)safe_malloc( nod->name_len+1 );
			if( nod->name ){
				memcpy( nod->name, name, nod->name_len );
				nod->name[nod->name_len] = '\0';
			}
			return nod;
		}
	}else{	//得意之作
		char *p, *m, *v;
		char* conds[16][2];
		int count, i, j, end;
		struct BXML_NODE* nod_attr;
		for( p=condition, i=0, end=0, v=condition; !end && i<16; p++ ){
			if( *p=='&' || *p=='\0' ){
				if(*p=='\0')	end=1;
				*p = '\0';
				m = strchr(v, '=' );
				if( m==NULL ){
					printf("## syntax error: lack '='.\n");
				}else{
					*m = '\0';
					conds[i][0] = v;
					conds[i][1] = m+1;
					i++;
				}
				v = p+1;
			}
		}
		//search nodes
		while( nod ){
			if( strcmp( nod->name, name )==0 ){
				//meet all conditions
				for( j=0; j<i; j++ ){
					nod_attr = get_attrnode( xml, nod, conds[j][0], 0 );
					if( !nod_attr || !nod_attr->value )
						break;
					if( strcmp( nod_attr->value, conds[j][1] )!=0 )
						break;
				}
				if( j==i ){	//均符合要求
					return nod;
				}
			}
			nod = nod->node_next;
		}
		//not found
		if( create ){
			nod = create_childnode( xml, parent );
			nod->name_len = strlen( name );
			nod->name = (char*)safe_malloc( nod->name_len+1 );
			if( nod->name ){
				memcpy( nod->name, name, nod->name_len );
				nod->name[nod->name_len] = '\0';
			}
			//meet all conditions
			for( j=0; j<i; j++ ){
				nod_attr = get_attrnode( xml, nod, conds[j][0], 1 );
				if( !nod_attr )
					break;
				nod_attr->value_len = strlen(conds[j][1]);
				nod_attr->value = (char*)safe_malloc( nod_attr->value_len+1);
				strcpy( nod_attr->value, conds[j][1] );
			}
			return nod;
		}
	}
	return NULL;
}

//change path
// "/a/b/c"
// "/a?id="3434"/b/c
static struct BXML_NODE* parse_path( struct BXML_DATA *xml, const char* path, int create )
{
	struct BXML_NODE* nod;
	const char* p, *v;
	char tmp[256];
	int len;
	p = path;
	//root 
	if( *p == '/' ){
		nod = xml->node_root;
		p++;
	}else{
		nod = xml->node_cur;
	}
	//
	v = p;
	for(;nod;p++){
		if( *p=='/' || *p=='\0' ){//进入一级目录
			char* m;
			len = (int)(p-v);
			if( len>0 && len<256 ){
				memcpy( tmp, v, len );
				tmp[len] = '\0';
				m = strchr( tmp, '?' );	
				if( m ){	//need search
					*m = '\0';
					if( *tmp ){
						nod = get_childnode( xml, nod, tmp, m+1, create );
					}else if( nod->node_parent ){
						nod = get_childnode( xml, nod->node_parent, nod->name, m+1, create );
					}
				}else{
					m = strchr( tmp, ':' );
					if( m ){
						*m = '\0';
						if( *tmp ) // if tmp != m
							nod = get_childnode( xml, nod, tmp, NULL, create );
						if( nod )
							nod = get_attrnode( xml, nod, m+1, create );
					}else{
						nod = get_childnode( xml, nod, tmp, NULL, create );
					}
				}
			}
			v = p+1;
			if( *p=='\0' )
				break;
		}
	}
	return nod;
}

//change the current path
//return 1:success  0:failed.
//
int bxml_redirect( struct BXML_DATA* xml, const char* path, int create_if_not_exist )	
{
	struct BXML_NODE* nod;
	nod = parse_path( xml, path, create_if_not_exist );
	if( nod ){
		xml->node_cur = nod;
		return 1;
	}else{
		return 0;
	}
}

//read 
int bxml_read( struct BXML_DATA* xml, const char* path, void* buf, unsigned int buf_size )
{
	struct BXML_NODE* nod;
	nod = parse_path( xml, path, 0 );
	if( nod && nod->value ){
		buf_size = nod->value_len < buf_size ? nod->value_len:buf_size;
		memcpy( buf, nod->value, buf_size );
		return buf_size;
	}else{
		return -1;	//temporary do this.
	}
}

//write 
void bxml_write( struct BXML_DATA* xml, const char* path, const void* buf, unsigned int buf_size )	
{
	struct BXML_NODE* nod;
	nod = parse_path( xml, path, 1 );
	if( nod ){
		if( nod->value )
			safe_free( nod->value );
		nod->value_len = buf_size;
		nod->value = (char*)safe_malloc( nod->value_len );
		if( nod->value )
			memcpy( nod->value, buf, nod->value_len );
	}
}

//read a name of a node
char* bxml_readname( struct BXML_DATA* xml, const char* path )	
{
	struct BXML_NODE* nod;
	nod = parse_path( xml, path, 0 );
	if( nod && nod->name ){
		return nod->name;
	}else{
		return NULL;	//temporary do this.
	}
}

//read a string
char* bxml_readstr( struct BXML_DATA* xml, const char* path )
{
	struct BXML_NODE* nod;
	nod = parse_path( xml, path, 0 );
	if( nod && nod->value ){
		return nod->value;
	}else{
		return NULL;	//temporary do this.
	}
}

//write a string
void bxml_writestr( struct BXML_DATA* xml, const char* path, const char* str )	
{
	struct BXML_NODE* nod;
	nod = parse_path( xml, path, 1 );
	if( nod ){
		if( nod->value )
			safe_free( nod->value );
		nod->value_len = strlen(str);
		nod->value = (char*)safe_malloc( nod->value_len+1 );
		if( nod->value )
			strcpy( nod->value, str );
	}
}

//move to the next node.
//return 1:succ 0:failed
int bxml_movenext( struct BXML_DATA* xml )
{
	struct BXML_NODE* nod;
	nod = xml->node_cur;
	if( nod ) 
		nod = nod->node_next;
	if( nod ){
		xml->node_cur = nod;
		return 1;
	}else{
		return 0;
	}
}

//创建 下一个项目
int bxml_mknode( struct BXML_DATA* xml, const char* name )
{
	struct BXML_NODE* nod;
	if( !xml->node_cur ) 
		return -1;
	//create one
	nod = create_nextnode( xml, xml->node_cur );
	if( nod ){
		nod->name_len = strlen(name);		//节点名长度
		nod->name = (char*)safe_malloc( nod->name_len+1 );
		if( nod->name ){
			memcpy( nod->name, name, nod->name_len );	//节点名
		nod->name[nod->name_len] = '\0';
	}
		xml->node_cur = nod;
		return 1;
	}else{
		return 0;
	}
}		
