#include "sceneinfotag.h"

#include <QJsonArray>

// InfoTagにRegisterダイアログから登録できるかどうかの情報も持たせる

QJsonObject SceneInfoTagDisplayProperties::toJson() const {
  QJsonObject obj;
  obj["labelTxt"] = labelTxt;
  obj["bgColor"]  = bgColor.name();
  obj["txtColor"] = txtColor.name();
  return obj;
}

SceneInfoTagDisplayProperties SceneInfoTagDisplayProperties::fromJson(
    const QJsonObject& obj) {
  SceneInfoTagDisplayProperties prop;
  if (const QJsonValue v = obj["labelTxt"]; v.isString())
    prop.labelTxt = v.toString();
  if (const QJsonValue v = obj["bgColor"]; v.isString())
    prop.bgColor = QColor::fromString(v.toString());
  if (const QJsonValue v = obj["txtColor"]; v.isString())
    prop.txtColor = QColor::fromString(v.toString());

  return prop;
}

//----------------------------------------------------

QJsonObject BoolSceneInfoTag::toJson() const {
  QJsonObject obj;
  obj["displayProperties"] = m_displayProperties.toJson();
  return obj;
}

BoolSceneInfoTag* BoolSceneInfoTag::fromJson(const QJsonObject& obj) {
  QString idName      = obj["idName"].toString();
  bool canManuallySet = obj["canManuallySet"].toBool();

  SceneInfoTagDisplayProperties prop;
  if (const QJsonValue v = obj["displayProperties"]; v.isObject())
    prop = SceneInfoTagDisplayProperties::fromJson(v.toObject());

  return new BoolSceneInfoTag(idName, canManuallySet, prop);
}

SceneInfoTag* BoolSceneInfoTag::clone() const {
  return new BoolSceneInfoTag(this->idName(), this->canManuallySet(),
                              this->displayProperties());
}

//----------------------------------------------------

QJsonObject EnumSceneInfoTag::toJson() const {
  QJsonObject obj;
  QJsonArray dispPropArray;
  for (auto valName : m_displayPropertiesMap.keys()) {
    QJsonObject propObj  = m_displayPropertiesMap.value(valName).toJson();
    propObj["valueName"] = valName;
    dispPropArray.append(propObj);
  }
  obj["displayPropertiesMap"] = dispPropArray;
  return obj;
}

EnumSceneInfoTag* EnumSceneInfoTag::fromJson(const QJsonObject& obj) {
  QString idName        = obj["idName"].toString();
  bool canManuallySet   = obj["canManuallySet"].toBool();
  EnumSceneInfoTag* ret = new EnumSceneInfoTag(idName, canManuallySet);

  if (const QJsonValue v = obj["displayPropertiesMap"]; v.isArray()) {
    QJsonArray dispPropArray = v.toArray();
    for (const QJsonValue& propVal : dispPropArray) {
      QJsonObject propObj = propVal.toObject();
      QString valName;
      if (const QJsonValue v = propObj["valueName"]; v.isString())
        valName = v.toString();
      SceneInfoTagDisplayProperties prop =
          SceneInfoTagDisplayProperties::fromJson(propObj);
      ret->addItem(valName, prop);
    }
  }

  return ret;
}

SceneInfoTag* EnumSceneInfoTag::clone() const {
  EnumSceneInfoTag* ret =
      new EnumSceneInfoTag(this->idName(), this->canManuallySet());
  for (auto val : this->valueList()) {
    ret->addItem(val, this->displayProperties(val));
  }
  return ret;
}
//----------------------------------------------------

QJsonObject StringSceneInfoTag::toJson() const {
  QJsonObject obj;
  obj["displayProperties"] = m_displayProperties.toJson();
  return obj;
}

StringSceneInfoTag* StringSceneInfoTag::fromJson(const QJsonObject& obj) {
  QString idName      = obj["idName"].toString();
  bool canManuallySet = obj["canManuallySet"].toBool();

  SceneInfoTagDisplayProperties prop;
  if (const QJsonValue v = obj["displayProperties"]; v.isObject())
    prop = SceneInfoTagDisplayProperties::fromJson(v.toObject());

  return new StringSceneInfoTag(idName, canManuallySet, prop);
}

SceneInfoTag* StringSceneInfoTag::clone() const {
  return new StringSceneInfoTag(this->idName(), this->canManuallySet(),
                                this->displayProperties());
}
//----------------------------------------------------

QJsonObject StringListSceneInfoTag::toJson() const {
  QJsonObject obj;
  obj["displayProperties"] = m_displayProperties.toJson();
  return obj;
}

StringListSceneInfoTag* StringListSceneInfoTag::fromJson(
    const QJsonObject& obj) {
  QString idName      = obj["idName"].toString();
  bool canManuallySet = obj["canManuallySet"].toBool();

  SceneInfoTagDisplayProperties prop;
  if (const QJsonValue v = obj["displayProperties"]; v.isObject())
    prop = SceneInfoTagDisplayProperties::fromJson(v.toObject());

  return new StringListSceneInfoTag(idName, canManuallySet, prop);
}

SceneInfoTag* StringListSceneInfoTag::clone() const {
  return new StringListSceneInfoTag(this->idName(), this->canManuallySet(),
                                    this->displayProperties());
}