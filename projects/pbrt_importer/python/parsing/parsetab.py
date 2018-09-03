
# parsetab.py
# This file is automatically generated. Do not edit.
# pylint: disable=W,C,R
_tabversion = '3.10'

_lr_method = 'LALR'

_lr_signature = 'statements_sceneACCELERATOR AREA_LIGHT_SOURCE ATTRIBUTE_BEGIN ATTRIBUTE_END CAMERA COMMENT CONCAT_TRANSFORM COORDINATE_SYSTEM COORDINATE_SYSTEM_TRANSFORM FILM FILTER IDENTITY INCLUDE INTEGRATOR LIGHT_SOURCE LOOKAT L_SQUARE_BRACKET MAKE_NAMED_MATERIAL MATERIAL NAMED_MATERIAL NUMBER OBJECT_BEGIN OBJECT_END OBJECT_INSTANCE REVERSE_ORIENTATION ROTATE R_SQUARE_BRACKET SAMPLER SCALE SHAPE STRING TEXTURE TRANSFORM TRANSFORM_BEGIN TRANSFORM_END TRANSLATE WORLD_BEGIN WORLD_ENDstatement_main : statements_config world_begin statements_scene WORLD_ENDbasic_data_type : STRING\n                       | NUMBERdata_list_items : data_list_items basic_data_type\n                      | statements_config : statements_config statement_config\n                         | data_list : L_SQUARE_BRACKET data_list_items R_SQUARE_BRACKETargument : STRING basic_data_type\n                | STRING data_liststatements_scene : statements_scene statement_scene\n                         | statement_include : INCLUDE STRINGstatement_scene : statement_includestatement_config : statement_includearguments : arguments argument\n                 | world_begin : WORLD_BEGINstatement_scene : ATTRIBUTE_BEGINstatement_scene : ATTRIBUTE_ENDstatement_scene : TRANSFORM_BEGINstatement_scene : TRANSFORM_ENDstatement_scene : statement_transformstatement_transform : IDENTITYstatement_transform : TRANSLATE NUMBER NUMBER NUMBERstatement_transform : SCALE NUMBER NUMBER NUMBERstatement_transform : ROTATE NUMBER NUMBER NUMBER NUMBERstatement_transform : LOOKAT NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBERstatement_transform : COORDINATE_SYSTEM STRINGstatement_transform : COORDINATE_SYSTEM_TRANSFORM STRINGstatement_transform : TRANSFORM L_SQUARE_BRACKET NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER R_SQUARE_BRACKETstatement_transform : CONCAT_TRANSFORM L_SQUARE_BRACKET NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER R_SQUARE_BRACKETstatement_scene : MATERIAL STRING argumentsstatement_scene : NAMED_MATERIAL STRINGstatement_scene : MAKE_NAMED_MATERIAL STRING argumentsstatement_scene : TEXTURE STRING STRING STRING argumentsstatement_scene : LIGHT_SOURCE STRING argumentsstatement_scene : AREA_LIGHT_SOURCE STRING argumentsstatement_scene : OBJECT_BEGIN STRINGstatement_scene : OBJECT_ENDstatement_scene : OBJECT_INSTANCE STRINGstatement_scene : SHAPE STRING argumentsstatement_scene : REVERSE_ORIENTATIONstatement_config : statement_transformstatement_config : CAMERA STRING argumentsstatement_config : SAMPLER STRING argumentsstatement_config : FILM STRING argumentsstatement_config : FILTER STRING argumentsstatement_config : INTEGRATOR STRING argumentsstatement_config : ACCELERATOR STRING arguments'
    
_lr_action_items = {'ATTRIBUTE_BEGIN':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,4,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'ATTRIBUTE_END':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,5,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'TRANSFORM_BEGIN':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,6,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'TRANSFORM_END':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,7,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'MATERIAL':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,9,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'NAMED_MATERIAL':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,10,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'MAKE_NAMED_MATERIAL':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,11,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'TEXTURE':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,12,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'LIGHT_SOURCE':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,13,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'AREA_LIGHT_SOURCE':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,14,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'OBJECT_BEGIN':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,15,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'OBJECT_END':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,16,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'OBJECT_INSTANCE':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,17,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'SHAPE':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,18,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'REVERSE_ORIENTATION':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,19,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'INCLUDE':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,20,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'IDENTITY':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,21,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'TRANSLATE':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,22,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'SCALE':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,23,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'ROTATE':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,24,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'LOOKAT':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,25,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'COORDINATE_SYSTEM':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,26,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'COORDINATE_SYSTEM_TRANSFORM':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,27,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'TRANSFORM':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,28,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'CONCAT_TRANSFORM':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,29,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'$end':([0,1,2,3,4,5,6,7,8,16,19,21,30,31,32,34,35,36,37,38,39,44,45,48,49,51,52,53,61,62,63,64,69,70,71,72,74,75,83,94,113,114,],[-12,0,-11,-14,-19,-20,-21,-22,-23,-40,-43,-24,-17,-34,-17,-17,-17,-39,-41,-17,-13,-29,-30,-33,-35,-37,-38,-42,-16,-17,-25,-26,-2,-9,-10,-3,-36,-27,-8,-28,-31,-32,]),'STRING':([9,10,11,12,13,14,15,17,18,20,26,27,30,32,33,34,35,38,48,49,50,51,52,53,60,61,62,69,70,71,72,73,74,79,83,84,],[30,31,32,33,34,35,36,37,38,39,44,45,-17,-17,50,-17,-17,-17,60,60,62,60,60,60,69,-16,-17,-2,-9,-10,-3,-5,60,69,-8,-4,]),'NUMBER':([22,23,24,25,40,41,42,43,46,47,54,55,56,57,58,59,60,65,66,67,68,69,72,73,76,77,78,79,80,81,82,84,85,86,87,88,89,90,91,92,93,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,],[40,41,42,43,54,55,56,57,58,59,63,64,65,66,67,68,72,75,76,77,78,-2,-3,-5,80,81,82,72,85,86,87,-4,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,]),'L_SQUARE_BRACKET':([28,29,60,],[46,47,73,]),'R_SQUARE_BRACKET':([69,72,73,79,84,111,112,],[-2,-3,-5,83,-4,113,114,]),}

_lr_action = {}
for _k, _v in _lr_action_items.items():
   for _x,_y in zip(_v[0],_v[1]):
      if not _x in _lr_action:  _lr_action[_x] = {}
      _lr_action[_x][_k] = _y
del _lr_action_items

_lr_goto_items = {'statements_scene':([0,],[1,]),'statement_scene':([1,],[2,]),'statement_include':([1,],[3,]),'statement_transform':([1,],[8,]),'arguments':([30,32,34,35,38,62,],[48,49,51,52,53,74,]),'argument':([48,49,51,52,53,74,],[61,61,61,61,61,61,]),'basic_data_type':([60,79,],[70,84,]),'data_list':([60,],[71,]),'data_list_items':([73,],[79,]),}

_lr_goto = {}
for _k, _v in _lr_goto_items.items():
   for _x, _y in zip(_v[0], _v[1]):
       if not _x in _lr_goto: _lr_goto[_x] = {}
       _lr_goto[_x][_k] = _y
del _lr_goto_items
_lr_productions = [
  ("S' -> statements_scene","S'",1,None,None,None),
  ('statement_main -> statements_config world_begin statements_scene WORLD_END','statement_main',4,'p_statement_main','parser.py',23),
  ('basic_data_type -> STRING','basic_data_type',1,'p_basic_data_type','parser_basics.py',25),
  ('basic_data_type -> NUMBER','basic_data_type',1,'p_basic_data_type','parser_basics.py',26),
  ('data_list_items -> data_list_items basic_data_type','data_list_items',2,'p_data_list_items','parser_basics.py',31),
  ('data_list_items -> <empty>','data_list_items',0,'p_data_list_items','parser_basics.py',32),
  ('statements_config -> statements_config statement_config','statements_config',2,'p_statements_config','parser.py',36),
  ('statements_config -> <empty>','statements_config',0,'p_statements_config','parser.py',37),
  ('data_list -> L_SQUARE_BRACKET data_list_items R_SQUARE_BRACKET','data_list',3,'p_data_list','parser_basics.py',40),
  ('argument -> STRING basic_data_type','argument',2,'p_argument','parser_basics.py',45),
  ('argument -> STRING data_list','argument',2,'p_argument','parser_basics.py',46),
  ('statements_scene -> statements_scene statement_scene','statements_scene',2,'p_statements_scene','parser.py',49),
  ('statements_scene -> <empty>','statements_scene',0,'p_statements_scene','parser.py',50),
  ('statement_include -> INCLUDE STRING','statement_include',2,'p_statement_include','parser.py',59),
  ('statement_scene -> statement_include','statement_scene',1,'p_statements_scene_include','parser.py',92),
  ('statement_config -> statement_include','statement_config',1,'p_statements_config_include','parser.py',97),
  ('arguments -> arguments argument','arguments',2,'p_arguments','parser_basics.py',115),
  ('arguments -> <empty>','arguments',0,'p_arguments','parser_basics.py',116),
  ('world_begin -> WORLD_BEGIN','world_begin',1,'p_statement_world_begin','parser.py',134),
  ('statement_scene -> ATTRIBUTE_BEGIN','statement_scene',1,'p_statement_attribute_begin','parser.py',142),
  ('statement_scene -> ATTRIBUTE_END','statement_scene',1,'p_statement_attribute_end','parser.py',149),
  ('statement_scene -> TRANSFORM_BEGIN','statement_scene',1,'p_statement_transform_begin','parser.py',156),
  ('statement_scene -> TRANSFORM_END','statement_scene',1,'p_statement_transform_end','parser.py',162),
  ('statement_scene -> statement_transform','statement_scene',1,'p_statement_scene_transform','parser.py',168),
  ('statement_transform -> IDENTITY','statement_transform',1,'p_statement_transform_identity','parser.py',173),
  ('statement_transform -> TRANSLATE NUMBER NUMBER NUMBER','statement_transform',4,'p_statement_transform_translate','parser.py',179),
  ('statement_transform -> SCALE NUMBER NUMBER NUMBER','statement_transform',4,'p_statement_transform_scale','parser.py',186),
  ('statement_transform -> ROTATE NUMBER NUMBER NUMBER NUMBER','statement_transform',5,'p_statement_transform_rotate','parser.py',193),
  ('statement_transform -> LOOKAT NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER','statement_transform',10,'p_statement_transform_lookat','parser.py',201),
  ('statement_transform -> COORDINATE_SYSTEM STRING','statement_transform',2,'p_statement_transform_coordinate_system','parser.py',210),
  ('statement_transform -> COORDINATE_SYSTEM_TRANSFORM STRING','statement_transform',2,'p_statement_transform_coord_sys_transform','parser.py',216),
  ('statement_transform -> TRANSFORM L_SQUARE_BRACKET NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER R_SQUARE_BRACKET','statement_transform',19,'p_statement_transform','parser.py',222),
  ('statement_transform -> CONCAT_TRANSFORM L_SQUARE_BRACKET NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER R_SQUARE_BRACKET','statement_transform',19,'p_statement_transform_concat','parser.py',229),
  ('statement_scene -> MATERIAL STRING arguments','statement_scene',3,'p_statement_material','parser.py',236),
  ('statement_scene -> NAMED_MATERIAL STRING','statement_scene',2,'p_statement_named_material','parser.py',242),
  ('statement_scene -> MAKE_NAMED_MATERIAL STRING arguments','statement_scene',3,'p_statement_make_named_material','parser.py',255),
  ('statement_scene -> TEXTURE STRING STRING STRING arguments','statement_scene',5,'p_statement_texture','parser.py',264),
  ('statement_scene -> LIGHT_SOURCE STRING arguments','statement_scene',3,'p_statement_light','parser.py',279),
  ('statement_scene -> AREA_LIGHT_SOURCE STRING arguments','statement_scene',3,'p_statement_area_light','parser.py',288),
  ('statement_scene -> OBJECT_BEGIN STRING','statement_scene',2,'p_statement_object_begin','parser.py',296),
  ('statement_scene -> OBJECT_END','statement_scene',1,'p_statement_object_end','parser.py',302),
  ('statement_scene -> OBJECT_INSTANCE STRING','statement_scene',2,'p_statement_object_instance','parser.py',309),
  ('statement_scene -> SHAPE STRING arguments','statement_scene',3,'p_statement_shape','parser.py',316),
  ('statement_scene -> REVERSE_ORIENTATION','statement_scene',1,'p_statement_reverse_orientation','parser.py',343),
  ('statement_config -> statement_transform','statement_config',1,'p_statement_config_transform','parser.py',349),
  ('statement_config -> CAMERA STRING arguments','statement_config',3,'p_statement_camera','parser.py',354),
  ('statement_config -> SAMPLER STRING arguments','statement_config',3,'p_statement_sampler','parser.py',364),
  ('statement_config -> FILM STRING arguments','statement_config',3,'p_statement_film','parser.py',373),
  ('statement_config -> FILTER STRING arguments','statement_config',3,'p_statement_filter','parser.py',380),
  ('statement_config -> INTEGRATOR STRING arguments','statement_config',3,'p_statement_integrator','parser.py',387),
  ('statement_config -> ACCELERATOR STRING arguments','statement_config',3,'p_statement_accelerator','parser.py',396),
]
