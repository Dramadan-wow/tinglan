"""Synthetic migration regression; never opens the user's databases."""
import copy
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from retire_legacy_services import transform

def user(uid,name,service=0):
    return dict(id=uid,name=name,birth='2000-01-01',applied='2026-10-13',location='未设置',
                services=[dict(type=service,nick=name,password='unchanged-hash',friends=[],blocked=[])])
def fixture():
    a=user(10001,'Dramadan');b=user(10003,'王五',1)
    b.update(birth='2001-03-03',applied='2019-09-01',location='上海')
    a['services'][0]['friends']=[dict(id=10003,contact=1)]
    return dict(users=[a,b],groups=[dict(id=1,owner=10003,parent=0,members=[dict(id=10001,services=[0]),dict(id=10003,services=[1])],admins=[10003],pending=[],mutes=[])],messages=[dict(id='99',sender=10001,target=1,kind=1,source=0,destination=0,text='真实聊天',read=[10001,10003],stars=[10001])],requests=[],adminHash='same-admin-hash',adminLogs=[])

class CleanupTests(unittest.TestCase):
    def test_demo_removed_real_group_and_message_preserved(self):
        before=fixture();after,removed,report=transform(before)
        self.assertEqual(removed,{10003});self.assertEqual(after['groups'][0]['owner'],10001)
        self.assertEqual(after['messages'][0]['text'],'真实聊天');self.assertEqual(after['messages'][0]['read'],[10001])
        self.assertEqual(after['users'][0]['services'][0]['password'],'unchanged-hash')
        self.assertEqual(after['users'][0]['services'][0]['friends'],[])
        self.assertEqual(before['groups'][0]['owner'],10003)
    def test_renamed_original_id_preserved(self):
        d=fixture();d['users'][1]['name']='真实用户';d['users'][1]['services'][0]['nick']='真实用户'
        self.assertEqual(transform(d)[1],set())
    def test_same_name_new_id_preserved(self):
        d=fixture();d['users'].append(user(10007,'王五'))
        self.assertIn(10007,[u['id'] for u in transform(d)[0]['users']])
    def test_legacy_services_removed_from_real_user(self):
        d=fixture();d['users'][0]['services'].append(dict(type=3,nick='Dramadan',password='unused',friends=[],blocked=[]))
        self.assertEqual([s['type'] for s in transform(d)[0]['users'][0]['services']],[0])
    def test_real_legacy_only_user_not_deleted(self):
        d=fixture();d['users'].append(user(10008,'真实用户',6))
        with self.assertRaises(RuntimeError): transform(d)
    def test_idempotent(self):
        first=transform(fixture())[0];self.assertEqual(transform(first)[0],first)

if __name__=='__main__':unittest.main()
