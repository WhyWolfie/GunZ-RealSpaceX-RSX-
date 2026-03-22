#include "stdafx.h"
#include "RMesh.h"
#include "RAnimationFile.h"

#include "RealSpace2.h"

#include "MZFileSystem.h"


#include "../../FileSystem/FileSystem.h"

_USING_NAMESPACE_REALSPACE2

_NAMESPACE_REALSPACE2_BEGIN

RAnimationFile::RAnimationFile()
{
//	m_filename[0]	= 0;
//	m_NameID		= -1;

	m_ani_node		= NULL;
	m_ani_node_cnt	= 0;
	m_ani_type		= RAniType_Bone;

	m_pBipRootNode = NULL;

	m_max_frame		= 0;
	m_nRefCount		= 0;

	AddRef();
}

RAnimationFile::~RAnimationFile()
{
	if( m_ani_node ) {
		for(int i=0;i<m_ani_node_cnt;i++) {
			delete m_ani_node[i];
		}
		delete[] m_ani_node;
		m_ani_node = NULL;
	}

	DecRef();

	if(m_nRefCount != 0) {//누수..

	}
}

void RAnimationFile::AddRef()
{
	m_nRefCount++;
}

void RAnimationFile::DecRef()
{
	m_nRefCount--;

	if(m_nRefCount==0) {//지워야 할까?

	}
}

/*
char* RAnimationFile::GetName() {
	return m_filename;
}

void RAnimationFile::SetName(char* name) {
	if(!name) return;
	strcpy( m_filename , name );
}
*/

RAnimationNode* RAnimationFile::GetNode(char* name)
{
	for(int i=0;i<m_ani_node_cnt;i++) {
		if(m_ani_node[i]->CheckName(name))
			return m_ani_node[i];
	}
	return NULL;
}

bool RAnimationFile::LoadAni(char* filename)
{
	int node_cnt=0;

	char t_mesh_name[256];

	std::unique_ptr<IFile> pFile = std::unique_ptr<IFile>(getFileManager()->open(filename));
    if (!pFile.get())
    {
        return false;
    }

	ex_ani_t t_hd;

	pFile->read(sizeof(ex_ani_t), &t_hd);

	// 0,2 은 공통 본과 tm 계층에니
	// 1 는 버텍스
	// 3 은 메트릭스 샘플링

	DWORD ver = t_hd.ver;

	m_ani_node_cnt = t_hd.model_num;

	if( m_ani_node_cnt == 0 ) {
		mlog("키가 없는 %s 에니메이션 사용\n",filename);
		return false;
	}

	m_ani_node = new RAnimationNode*[m_ani_node_cnt];

	RAnimationNode* pANode = NULL;

	int vis_max_frame = 0;
	int max_frame = 0;

	int mode = t_hd.ani_type;

	m_ani_type = (AnimationType)mode;

	if( mode == RAniType_Vertex) {

		int i,j,vcnt;

		for(i=0;i<m_ani_node_cnt;i++) {

			m_ani_node[i] = new RAnimationNode;

			pANode = m_ani_node[i];

			pFile->read(MAX_NAME_LEN, t_mesh_name);
//			strcpy(pANode->m_Name,t_mesh_name);
			pANode->SetName(t_mesh_name);

			//			pANode->ConnectToNameID();

			pFile->read(4, &pANode->m_vertex_cnt);//frame수만큼

			if(pANode->m_vertex_cnt) {
				pANode->m_vertex = new D3DXVECTOR3*[pANode->m_vertex_cnt];
			}

			pFile->read(4, &vcnt);

			pANode->m_vertex_vcnt = vcnt;

			if(pANode->m_vertex_cnt) {
				pANode->m_vertex_frame = new DWORD[pANode->m_vertex_cnt];
			}

			pFile->read(sizeof(DWORD)*pANode->m_vertex_cnt, pANode->m_vertex_frame);

			for(j=0;j<pANode->m_vertex_cnt;j++) {

				pANode->m_vertex[j] = new D3DXVECTOR3[vcnt];
				pFile->read(sizeof(D3DXVECTOR3)*vcnt, pANode->m_vertex[j]);
			}

			if(ver > EXPORTER_ANI_VER1) {
				pFile->read(sizeof(DWORD), &pANode->m_vis_cnt);

				if(pANode->m_vis_cnt) {
					pANode->m_vis = new RVisKey[pANode->m_vis_cnt];
					pFile->read(sizeof(RVisKey)*pANode->m_vis_cnt, pANode->m_vis);

					if(pANode->m_vis[pANode->m_vis_cnt-1].frame > vis_max_frame) {
						vis_max_frame = pANode->m_vis[ pANode->m_vis_cnt-1 ].frame;
					}
				}
			}
		}

		int cnt = 0;

		// 더미노드가 섞여있는 경우가 있다..

		for(i=0;i<m_ani_node_cnt;i++) {

			cnt = m_ani_node[i]->m_vertex_cnt;

			if(cnt) {
				max_frame = m_ani_node[i]->m_vertex_frame[cnt-1];
				break;
			}
		}

	}
	else if( mode == RAniType_Tm ) {

		int i;
		for( i=0;i<m_ani_node_cnt;i++) {

			m_ani_node[i] = new RAnimationNode;

			pANode = m_ani_node[i];

			pFile->read(MAX_NAME_LEN, t_mesh_name);
//			strcpy(pANode->m_Name,t_mesh_name);
			pANode->SetName(t_mesh_name);
			//			pANode->ConnectToNameID();

			pFile->read(4, &pANode->m_mat_cnt);
			pANode->m_mat = new RTMKey[pANode->m_mat_cnt];

			pFile->read(sizeof(RTMKey)*pANode->m_mat_cnt, pANode->m_mat);

			if(ver > EXPORTER_ANI_VER1) {
				pFile->read(sizeof(DWORD), &pANode->m_vis_cnt);

				if(pANode->m_vis_cnt) {
					pANode->m_vis = new RVisKey[pANode->m_vis_cnt];
					pFile->read(sizeof(RVisKey)*pANode->m_vis_cnt, pANode->m_vis);

					if(pANode->m_vis[pANode->m_vis_cnt-1].frame > vis_max_frame) {
						vis_max_frame = pANode->m_vis[ pANode->m_vis_cnt-1 ].frame;
					}
				}
			}
			pANode->m_mat_base = pANode->m_mat[0];//.m;
		}

		int cnt = 0;

		// 더미노드가 섞여있는 경우가 있다..

		for(i=0;i<m_ani_node_cnt;i++) {

			cnt = m_ani_node[i]->m_mat_cnt;

			if(cnt) {
				max_frame = m_ani_node[i]->m_mat[cnt-1].frame;
				break;
			}
		}

	}
	else {

		for(int i=0;i<m_ani_node_cnt;i++) {

			m_ani_node[i] = new RAnimationNode;

			pANode = m_ani_node[i];

			pFile->read(MAX_NAME_LEN, t_mesh_name);
			pFile->read(sizeof(D3DXMATRIX), &pANode->m_mat_base);//mat

//			strcpy(pANode->m_Name,t_mesh_name);
			pANode->SetName(t_mesh_name);

			if(strcmp(pANode->GetName(),"Bip01")==0) {
				m_pBipRootNode = pANode;
			}

			//			pANode->ConnectToNameID();

			int pos_key_num = 0;
			int rot_key_num = 0;
			int vertex_num	= 0;

			pFile->read(4, &pos_key_num);

			pANode->m_pos_cnt = pos_key_num;

			if(pos_key_num) {

				pANode->m_pos = new RPosKey[pos_key_num+1];

				pFile->read(sizeof(RPosKey)*pos_key_num, pANode->m_pos);

				pANode->m_pos[pos_key_num] = pANode->m_pos[pos_key_num-1];

				if(pANode->m_pos[pos_key_num].frame > max_frame) {
					max_frame = pANode->m_pos[pos_key_num].frame;
				}
			}

			pFile->read(4, &rot_key_num);

			pANode->m_rot_cnt = rot_key_num;

			if(rot_key_num) {

				RQuatKey q;

				D3DXQUATERNION eq,q1,q2;

				pANode->m_quat = new RQuatKey[rot_key_num+1];

				RRotKey t_rk;

				for(int j=0;j<rot_key_num;j++) {

					if(ver > EXPORTER_ANI_VER3) {

						pFile->read(sizeof(RQuatKey), &pANode->m_quat[j]);

					} else {// old

						pFile->read(sizeof(RRotKey), &t_rk);

						RRot2Quat(q,t_rk);

						memcpy(&pANode->m_quat[j] , &q , sizeof(RQuatKey));
						pANode->m_quat[j].frame  = t_rk.frame;
					}
				}

				pANode->m_quat[rot_key_num] = pANode->m_quat[rot_key_num-1];

				if(pANode->m_quat[rot_key_num].frame > max_frame) {
					max_frame = pANode->m_quat[rot_key_num].frame;
				}
			}

			if(ver > EXPORTER_ANI_VER1) {
				pFile->read(sizeof(DWORD), &pANode->m_vis_cnt);

				if(pANode->m_vis_cnt) {
					pANode->m_vis = new RVisKey[pANode->m_vis_cnt];
					pFile->read(sizeof(RVisKey)*pANode->m_vis_cnt, pANode->m_vis);

					if(pANode->m_vis[pANode->m_vis_cnt-1].frame > vis_max_frame) {
						vis_max_frame = pANode->m_vis[ pANode->m_vis_cnt-1 ].frame;
					}
				}
			}
		}
	}

	m_max_frame = max_frame;

	if(m_max_frame < vis_max_frame) {
		m_max_frame = vis_max_frame;
	}


	return true;
}

_NAMESPACE_REALSPACE2_END