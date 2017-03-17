//
//  enginxServerProcessor.cpp
//  enginx
//
//  Created by stephenw on 2017/3/16.
//  Copyright © 2017年 stephenw.cc. All rights reserved.
//

#include "enginxServerProcessor.h"
#include "enginxLocationProcessor.h"
ENGINX_NAMESPACE_BEGIN
using namespace rapidjson;
using namespace std;

void EnginxServerVarsGenerator(map<string, string>& vars, EnginxURL const url) {
  vars[ENGINX_CONFIG_VAR_DEF_SCHEME] = url.schema;
  vars[ENGINX_CONFIG_VAR_DEF_HOST] = url.host;
  vars[ENGINX_CONFIG_VAR_DEF_QUERY_STRING] = url.querystring;
  vars[ENGINX_CONFIG_VAR_DEF_FRAGMENT] = url.fragment;
  vars[ENGINX_CONFIG_VAR_DEF_REQUEST_URI] = url.path + "?" +url.querystring + url.fragment;
}

EnginxServerProcessor::EnginxServerProcessor(Value& server, EnginxURL const url) {
  can_continue_rewrite = false;
  rewrited_url = url.absolute_url;
  current_url = url;
  current_server.CopyFrom(server, current_server.GetAllocator());
  EnginxServerVarsGenerator(internal_vars, url);
  //resolve and exec instructions
  if (!resolveActions()) { return; };
  if (!server.HasMember(ENGINX_CONFIG_FIELD_LOCATION)) { return; }
  Value& locations = server[ENGINX_CONFIG_FIELD_LOCATION];
  if (!locations.IsObject()) {
    return;
  }
  EnginxLocationDispatcher(url, locations, internal_vars, rewrited_url);
}


/**
 action resolve failed or returned returns false

 @return wheather process next location parse
 */
bool EnginxServerProcessor::resolveActions() {
  if (!current_server.HasMember(ENGINX_CONFIG_FIELD_ACTION)) {
    return true;
  }
  Value& actions = current_server[ENGINX_CONFIG_FIELD_ACTION];
  if (actions.IsString()) {
    return resolveInstruction(actions.GetString());
  }
  if (actions.IsArray()) {
    bool continue_next = true;
    for (Value::ValueIterator itr = actions.Begin(); itr < actions.End(); ++itr) {
      if (itr->IsString()) {
        continue_next = resolveInstruction(itr->GetString());
        if (!continue_next) return false;
      } else {
        continue_next = false;
        break;
      }
    }
    return continue_next;
  }
  return true;
}

bool EnginxServerProcessor::resolveInstruction(std::string instruction) {
  vector<string> parts;
  SplitString(instruction, parts, " ");
  if (parts.size() == 0) {
    return true;
  }
//  matches "return"
  if (parts.size() == 1) {
    return !(parts[0].compare(ENGINX_CONFIG_INSTRUCTION_RETURN) == 0);
  }
  if (parts.size() == 2 || parts.size() == 3) {
    return execInstruction(parts);
  }
  return true;
}


void EnginxServerProcessor::compileTemplateString(string& template_str) {
  map<string, string>::iterator itr;
  for (itr = internal_vars.begin(); itr != internal_vars.end(); ++itr) {
    string::size_type p = template_str.find(itr->first);
    if (p != string::npos) {
      template_str.replace(p, itr->first.length(), itr->second);
    }
  }
}

void EnginxServerProcessor::compileInternalVars(vector<string>& parts) {
  if (parts.size() != 2) {
    return;
  }
  map<string, string>::iterator itr;
  for (itr = internal_vars.begin(); itr != internal_vars.end(); ++itr) {
    if (itr->first.compare(parts[1]) == 0) {
      if (parts[0].compare(ENGINX_CONFIG_INSTRUCTION_ENCODE)) {
        itr->second = UrlEncode(itr->second);
      } else if (parts[0].compare(ENGINX_CONFIG_INSTRUCTION_DECODE)) {
        itr->second = UrlDecode(itr->second);
      }
    }
  }
}

bool EnginxServerProcessor::execInstruction(vector<string>& parts) {
  //matches "return x temporarily"
  if (parts.size() == 3) {
    if (parts[0].compare(ENGINX_CONFIG_INSTRUCTION_RETURN) == 0 &&
        parts[2].compare(ENGINX_CONFIG_INSTRUCTION_TEMPORARILY) == 0) {
      string result_str = parts[1];
      compileTemplateString(result_str);
      rewrited_url = result_str;
      can_continue_rewrite = true;
      return false;
    } else {
      //other 3 parts instructions do nothing
      return true;
    }
  } else if (parts.size() == 2) {
    string result_str = parts[1];
    if (parts[0].compare(ENGINX_CONFIG_INSTRUCTION_RETURN) == 0) {
      compileTemplateString(result_str);
      rewrited_url = result_str;
      can_continue_rewrite = false;
      return false;
    } else if (parts[0].compare(ENGINX_CONFIG_INSTRUCTION_ENCODE) == 0 ||
               parts[0].compare(ENGINX_CONFIG_INSTRUCTION_DECODE) == 0) {
      compileInternalVars(parts);
    } else {
      //unrecognized instruction
      return true;
    }
    return true;
  }
  
  return true;
}

ENGINX_NAMESPACE_END
