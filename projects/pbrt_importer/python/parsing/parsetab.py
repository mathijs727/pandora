
# parsetab.py
# This file is automatically generated. Do not edit.
# pylint: disable=W,C,R
_tabversion = '3.10'

_lr_method = 'LALR'

_lr_signature = 'ACCELERATOR AREA_LIGHT_SOURCE ATTRIBUTE_BEGIN ATTRIBUTE_END CAMERA COMMENT CONCAT_TRANSFORM COORDINATE_SYSTEM COORDINATE_SYSTEM_TRANSFORM FILM FILTER IDENTITY INCLUDE INTEGRATOR LIGHT_SOURCE LOOKAT L_SQUARE_BRACKET MAKE_NAMED_MATERIAL MATERIAL NAMED_MATERIAL NUMBER OBJECT_BEGIN OBJECT_END OBJECT_INSTANCE REVERSE_ORIENTATION ROTATE R_SQUARE_BRACKET SAMPLER SCALE SHAPE STRING TEXTURE TRANSFORM TRANSFORM_BEGIN TRANSFORM_END TRANSLATE WORLD_BEGIN WORLD_ENDstatement_main : statements_config world_begin statements_scene WORLD_ENDstatements_config : statements_config statement_config\n                         | basic_data_type : STRING\n                       | NUMBERdata_list_items : data_list_items basic_data_type\n                      | statements_scene : statements_scene statement_scene\n                         | data_list : L_SQUARE_BRACKET data_list_items R_SQUARE_BRACKETargument : STRING basic_data_type\n                | STRING data_listworld_begin : WORLD_BEGINstatement_scene : ATTRIBUTE_BEGINstatement_scene : ATTRIBUTE_ENDstatement_scene : TRANSFORM_BEGINstatement_scene : TRANSFORM_ENDstatement_scene : statement_transformstatement_transform : IDENTITYarguments : arguments argument\n                 | statement_transform : TRANSLATE NUMBER NUMBER NUMBERstatement_transform : SCALE NUMBER NUMBER NUMBERstatement_transform : ROTATE NUMBER NUMBER NUMBER NUMBERstatement_transform : LOOKAT NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBERstatement_transform : COORDINATE_SYSTEM STRINGstatement_transform : COORDINATE_SYSTEM_TRANSFORM STRINGstatement_transform : TRANSFORM L_SQUARE_BRACKET NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER R_SQUARE_BRACKETstatement_transform : CONCAT_TRANSFORM NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBERstatement_scene : MATERIAL STRING argumentsstatement_scene : NAMED_MATERIAL STRINGstatement_scene : MAKE_NAMED_MATERIAL STRING argumentsstatement_scene : TEXTURE STRING STRING STRING argumentsstatement_scene : LIGHT_SOURCE STRING argumentsstatement_scene : AREA_LIGHT_SOURCE STRING argumentsstatement_scene : OBJECT_BEGIN STRINGstatement_scene : OBJECT_ENDstatement_scene : OBJECT_INSTANCE STRINGstatement_scene : SHAPE STRING argumentsstatement_scene : REVERSE_ORIENTATIONstatement_config : statement_transformstatement_config : CAMERA STRING argumentsstatement_config : SAMPLER STRING argumentsstatement_config : FILM STRING argumentsstatement_config : FILTER STRING argumentsstatement_config : INTEGRATOR STRING argumentsstatement_config : ACCELERATOR STRING arguments'
    
_lr_action_items = {'WORLD_BEGIN':([0,2,4,6,13,23,24,25,26,27,28,33,34,55,56,57,58,59,60,77,78,79,90,91,92,93,95,105,116,132,134,],[-3,5,-2,-41,-19,-21,-21,-21,-21,-21,-21,-26,-27,-42,-43,-44,-45,-46,-47,-20,-22,-23,-4,-11,-12,-5,-24,-10,-25,-29,-28,]),'CAMERA':([0,2,4,6,13,23,24,25,26,27,28,33,34,55,56,57,58,59,60,77,78,79,90,91,92,93,95,105,116,132,134,],[-3,7,-2,-41,-19,-21,-21,-21,-21,-21,-21,-26,-27,-42,-43,-44,-45,-46,-47,-20,-22,-23,-4,-11,-12,-5,-24,-10,-25,-29,-28,]),'SAMPLER':([0,2,4,6,13,23,24,25,26,27,28,33,34,55,56,57,58,59,60,77,78,79,90,91,92,93,95,105,116,132,134,],[-3,8,-2,-41,-19,-21,-21,-21,-21,-21,-21,-26,-27,-42,-43,-44,-45,-46,-47,-20,-22,-23,-4,-11,-12,-5,-24,-10,-25,-29,-28,]),'FILM':([0,2,4,6,13,23,24,25,26,27,28,33,34,55,56,57,58,59,60,77,78,79,90,91,92,93,95,105,116,132,134,],[-3,9,-2,-41,-19,-21,-21,-21,-21,-21,-21,-26,-27,-42,-43,-44,-45,-46,-47,-20,-22,-23,-4,-11,-12,-5,-24,-10,-25,-29,-28,]),'FILTER':([0,2,4,6,13,23,24,25,26,27,28,33,34,55,56,57,58,59,60,77,78,79,90,91,92,93,95,105,116,132,134,],[-3,10,-2,-41,-19,-21,-21,-21,-21,-21,-21,-26,-27,-42,-43,-44,-45,-46,-47,-20,-22,-23,-4,-11,-12,-5,-24,-10,-25,-29,-28,]),'INTEGRATOR':([0,2,4,6,13,23,24,25,26,27,28,33,34,55,56,57,58,59,60,77,78,79,90,91,92,93,95,105,116,132,134,],[-3,11,-2,-41,-19,-21,-21,-21,-21,-21,-21,-26,-27,-42,-43,-44,-45,-46,-47,-20,-22,-23,-4,-11,-12,-5,-24,-10,-25,-29,-28,]),'ACCELERATOR':([0,2,4,6,13,23,24,25,26,27,28,33,34,55,56,57,58,59,60,77,78,79,90,91,92,93,95,105,116,132,134,],[-3,12,-2,-41,-19,-21,-21,-21,-21,-21,-21,-26,-27,-42,-43,-44,-45,-46,-47,-20,-22,-23,-4,-11,-12,-5,-24,-10,-25,-29,-28,]),'IDENTITY':([0,2,3,4,5,6,13,22,23,24,25,26,27,28,33,34,38,39,40,41,42,43,51,54,55,56,57,58,59,60,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-3,13,-9,-2,-13,-41,-19,13,-21,-21,-21,-21,-21,-21,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-42,-43,-44,-45,-46,-47,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'TRANSLATE':([0,2,3,4,5,6,13,22,23,24,25,26,27,28,33,34,38,39,40,41,42,43,51,54,55,56,57,58,59,60,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-3,14,-9,-2,-13,-41,-19,14,-21,-21,-21,-21,-21,-21,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-42,-43,-44,-45,-46,-47,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'SCALE':([0,2,3,4,5,6,13,22,23,24,25,26,27,28,33,34,38,39,40,41,42,43,51,54,55,56,57,58,59,60,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-3,15,-9,-2,-13,-41,-19,15,-21,-21,-21,-21,-21,-21,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-42,-43,-44,-45,-46,-47,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'ROTATE':([0,2,3,4,5,6,13,22,23,24,25,26,27,28,33,34,38,39,40,41,42,43,51,54,55,56,57,58,59,60,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-3,16,-9,-2,-13,-41,-19,16,-21,-21,-21,-21,-21,-21,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-42,-43,-44,-45,-46,-47,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'LOOKAT':([0,2,3,4,5,6,13,22,23,24,25,26,27,28,33,34,38,39,40,41,42,43,51,54,55,56,57,58,59,60,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-3,17,-9,-2,-13,-41,-19,17,-21,-21,-21,-21,-21,-21,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-42,-43,-44,-45,-46,-47,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'COORDINATE_SYSTEM':([0,2,3,4,5,6,13,22,23,24,25,26,27,28,33,34,38,39,40,41,42,43,51,54,55,56,57,58,59,60,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-3,18,-9,-2,-13,-41,-19,18,-21,-21,-21,-21,-21,-21,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-42,-43,-44,-45,-46,-47,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'COORDINATE_SYSTEM_TRANSFORM':([0,2,3,4,5,6,13,22,23,24,25,26,27,28,33,34,38,39,40,41,42,43,51,54,55,56,57,58,59,60,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-3,19,-9,-2,-13,-41,-19,19,-21,-21,-21,-21,-21,-21,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-42,-43,-44,-45,-46,-47,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'TRANSFORM':([0,2,3,4,5,6,13,22,23,24,25,26,27,28,33,34,38,39,40,41,42,43,51,54,55,56,57,58,59,60,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-3,20,-9,-2,-13,-41,-19,20,-21,-21,-21,-21,-21,-21,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-42,-43,-44,-45,-46,-47,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'CONCAT_TRANSFORM':([0,2,3,4,5,6,13,22,23,24,25,26,27,28,33,34,38,39,40,41,42,43,51,54,55,56,57,58,59,60,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-3,21,-9,-2,-13,-41,-19,21,-21,-21,-21,-21,-21,-21,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-42,-43,-44,-45,-46,-47,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'$end':([1,37,],[0,-1,]),'WORLD_END':([3,5,13,22,33,34,38,39,40,41,42,43,51,54,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-9,-13,-19,37,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'ATTRIBUTE_BEGIN':([3,5,13,22,33,34,38,39,40,41,42,43,51,54,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-9,-13,-19,39,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'ATTRIBUTE_END':([3,5,13,22,33,34,38,39,40,41,42,43,51,54,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-9,-13,-19,40,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'TRANSFORM_BEGIN':([3,5,13,22,33,34,38,39,40,41,42,43,51,54,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-9,-13,-19,41,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'TRANSFORM_END':([3,5,13,22,33,34,38,39,40,41,42,43,51,54,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-9,-13,-19,42,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'MATERIAL':([3,5,13,22,33,34,38,39,40,41,42,43,51,54,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-9,-13,-19,44,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'NAMED_MATERIAL':([3,5,13,22,33,34,38,39,40,41,42,43,51,54,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-9,-13,-19,45,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'MAKE_NAMED_MATERIAL':([3,5,13,22,33,34,38,39,40,41,42,43,51,54,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-9,-13,-19,46,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'TEXTURE':([3,5,13,22,33,34,38,39,40,41,42,43,51,54,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-9,-13,-19,47,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'LIGHT_SOURCE':([3,5,13,22,33,34,38,39,40,41,42,43,51,54,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-9,-13,-19,48,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'AREA_LIGHT_SOURCE':([3,5,13,22,33,34,38,39,40,41,42,43,51,54,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-9,-13,-19,49,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'OBJECT_BEGIN':([3,5,13,22,33,34,38,39,40,41,42,43,51,54,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-9,-13,-19,50,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'OBJECT_END':([3,5,13,22,33,34,38,39,40,41,42,43,51,54,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-9,-13,-19,51,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'OBJECT_INSTANCE':([3,5,13,22,33,34,38,39,40,41,42,43,51,54,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-9,-13,-19,52,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'SHAPE':([3,5,13,22,33,34,38,39,40,41,42,43,51,54,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-9,-13,-19,53,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'REVERSE_ORIENTATION':([3,5,13,22,33,34,38,39,40,41,42,43,51,54,67,68,69,71,72,73,74,75,77,78,79,84,85,87,88,89,90,91,92,93,95,99,104,105,116,132,134,],[-9,-13,-19,54,-26,-27,-8,-14,-15,-16,-17,-18,-37,-40,-21,-31,-21,-21,-21,-36,-38,-21,-20,-22,-23,-30,-32,-34,-35,-39,-4,-11,-12,-5,-24,-21,-33,-10,-25,-29,-28,]),'STRING':([7,8,9,10,11,12,18,19,23,24,25,26,27,28,44,45,46,47,48,49,50,52,53,55,56,57,58,59,60,67,69,70,71,72,75,76,77,84,85,86,87,88,89,90,91,92,93,94,99,100,104,105,106,],[23,24,25,26,27,28,33,34,-21,-21,-21,-21,-21,-21,67,68,69,70,71,72,73,74,75,76,76,76,76,76,76,-21,-21,86,-21,-21,-21,90,-20,76,76,99,76,76,76,-4,-11,-12,-5,-7,-21,90,76,-10,-6,]),'NUMBER':([14,15,16,17,21,29,30,31,32,35,36,61,62,63,64,65,66,76,80,81,82,83,90,93,94,96,97,98,100,101,102,103,106,107,108,109,110,111,112,113,114,115,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,],[29,30,31,32,36,61,62,63,64,65,66,78,79,80,81,82,83,93,95,96,97,98,-4,-5,-7,101,102,103,93,107,108,109,-6,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,]),'L_SQUARE_BRACKET':([20,76,],[35,94,]),'R_SQUARE_BRACKET':([90,93,94,100,106,133,],[-4,-5,-7,105,-6,134,]),}

_lr_action = {}
for _k, _v in _lr_action_items.items():
   for _x,_y in zip(_v[0],_v[1]):
      if not _x in _lr_action:  _lr_action[_x] = {}
      _lr_action[_x][_k] = _y
del _lr_action_items

_lr_goto_items = {'statement_main':([0,],[1,]),'statements_config':([0,],[2,]),'world_begin':([2,],[3,]),'statement_config':([2,],[4,]),'statement_transform':([2,22,],[6,43,]),'statements_scene':([3,],[22,]),'statement_scene':([22,],[38,]),'arguments':([23,24,25,26,27,28,67,69,71,72,75,99,],[55,56,57,58,59,60,84,85,87,88,89,104,]),'argument':([55,56,57,58,59,60,84,85,87,88,89,104,],[77,77,77,77,77,77,77,77,77,77,77,77,]),'basic_data_type':([76,100,],[91,106,]),'data_list':([76,],[92,]),'data_list_items':([94,],[100,]),}

_lr_goto = {}
for _k, _v in _lr_goto_items.items():
   for _x, _y in zip(_v[0], _v[1]):
       if not _x in _lr_goto: _lr_goto[_x] = {}
       _lr_goto[_x][_k] = _y
del _lr_goto_items
_lr_productions = [
  ("S' -> statement_main","S'",1,None,None,None),
  ('statement_main -> statements_config world_begin statements_scene WORLD_END','statement_main',4,'p_statement_main','parser.py',10),
  ('statements_config -> statements_config statement_config','statements_config',2,'p_statements_config','parser.py',23),
  ('statements_config -> <empty>','statements_config',0,'p_statements_config','parser.py',24),
  ('basic_data_type -> STRING','basic_data_type',1,'p_basic_data_type','parser_basics.py',23),
  ('basic_data_type -> NUMBER','basic_data_type',1,'p_basic_data_type','parser_basics.py',24),
  ('data_list_items -> data_list_items basic_data_type','data_list_items',2,'p_data_list_items','parser_basics.py',29),
  ('data_list_items -> <empty>','data_list_items',0,'p_data_list_items','parser_basics.py',30),
  ('statements_scene -> statements_scene statement_scene','statements_scene',2,'p_statements_scene','parser.py',36),
  ('statements_scene -> <empty>','statements_scene',0,'p_statements_scene','parser.py',37),
  ('data_list -> L_SQUARE_BRACKET data_list_items R_SQUARE_BRACKET','data_list',3,'p_data_list','parser_basics.py',38),
  ('argument -> STRING basic_data_type','argument',2,'p_argument','parser_basics.py',43),
  ('argument -> STRING data_list','argument',2,'p_argument','parser_basics.py',44),
  ('world_begin -> WORLD_BEGIN','world_begin',1,'p_statement_world_begin','parser.py',74),
  ('statement_scene -> ATTRIBUTE_BEGIN','statement_scene',1,'p_statement_attribute_begin','parser.py',81),
  ('statement_scene -> ATTRIBUTE_END','statement_scene',1,'p_statement_attribute_end','parser.py',88),
  ('statement_scene -> TRANSFORM_BEGIN','statement_scene',1,'p_statement_transform_begin','parser.py',95),
  ('statement_scene -> TRANSFORM_END','statement_scene',1,'p_statement_transform_end','parser.py',101),
  ('statement_scene -> statement_transform','statement_scene',1,'p_statement_scene_transform','parser.py',107),
  ('statement_transform -> IDENTITY','statement_transform',1,'p_statement_transform_identity','parser.py',112),
  ('arguments -> arguments argument','arguments',2,'p_arguments','parser_basics.py',113),
  ('arguments -> <empty>','arguments',0,'p_arguments','parser_basics.py',114),
  ('statement_transform -> TRANSLATE NUMBER NUMBER NUMBER','statement_transform',4,'p_statement_transform_translate','parser.py',118),
  ('statement_transform -> SCALE NUMBER NUMBER NUMBER','statement_transform',4,'p_statement_transform_scale','parser.py',125),
  ('statement_transform -> ROTATE NUMBER NUMBER NUMBER NUMBER','statement_transform',5,'p_statement_transform_rotate','parser.py',132),
  ('statement_transform -> LOOKAT NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER','statement_transform',10,'p_statement_transform_lookat','parser.py',140),
  ('statement_transform -> COORDINATE_SYSTEM STRING','statement_transform',2,'p_statement_transform_coordinate_system','parser.py',149),
  ('statement_transform -> COORDINATE_SYSTEM_TRANSFORM STRING','statement_transform',2,'p_statement_transform_coord_sys_transform','parser.py',155),
  ('statement_transform -> TRANSFORM L_SQUARE_BRACKET NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER R_SQUARE_BRACKET','statement_transform',19,'p_statement_transform','parser.py',161),
  ('statement_transform -> CONCAT_TRANSFORM NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER','statement_transform',17,'p_statement_transform_concat','parser.py',168),
  ('statement_scene -> MATERIAL STRING arguments','statement_scene',3,'p_statement_material','parser.py',180),
  ('statement_scene -> NAMED_MATERIAL STRING','statement_scene',2,'p_statement_named_material','parser.py',186),
  ('statement_scene -> MAKE_NAMED_MATERIAL STRING arguments','statement_scene',3,'p_statement_make_named_material','parser.py',196),
  ('statement_scene -> TEXTURE STRING STRING STRING arguments','statement_scene',5,'p_statement_texture','parser.py',205),
  ('statement_scene -> LIGHT_SOURCE STRING arguments','statement_scene',3,'p_statement_light','parser.py',218),
  ('statement_scene -> AREA_LIGHT_SOURCE STRING arguments','statement_scene',3,'p_statement_area_light','parser.py',227),
  ('statement_scene -> OBJECT_BEGIN STRING','statement_scene',2,'p_statement_object_begin','parser.py',235),
  ('statement_scene -> OBJECT_END','statement_scene',1,'p_statement_object_end','parser.py',241),
  ('statement_scene -> OBJECT_INSTANCE STRING','statement_scene',2,'p_statement_object_instance','parser.py',248),
  ('statement_scene -> SHAPE STRING arguments','statement_scene',3,'p_statement_shape','parser.py',254),
  ('statement_scene -> REVERSE_ORIENTATION','statement_scene',1,'p_statement_reverse_orientation','parser.py',271),
  ('statement_config -> statement_transform','statement_config',1,'p_statement_config_transform','parser.py',277),
  ('statement_config -> CAMERA STRING arguments','statement_config',3,'p_statement_camera','parser.py',282),
  ('statement_config -> SAMPLER STRING arguments','statement_config',3,'p_statement_sampler','parser.py',292),
  ('statement_config -> FILM STRING arguments','statement_config',3,'p_statement_film','parser.py',300),
  ('statement_config -> FILTER STRING arguments','statement_config',3,'p_statement_filter','parser.py',307),
  ('statement_config -> INTEGRATOR STRING arguments','statement_config',3,'p_statement_integrator','parser.py',314),
  ('statement_config -> ACCELERATOR STRING arguments','statement_config',3,'p_statement_accelerator','parser.py',323),
]