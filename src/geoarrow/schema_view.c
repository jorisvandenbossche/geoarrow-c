
#include <errno.h>
#include <stddef.h>
#include <string.h>

#include "geoarrow.h"
#include "nanoarrow.h"

static GeoArrowErrorCode GeoArrowParseNestedSchema(struct ArrowSchema* schema, int n,
                                                   struct GeoArrowSchemaView* schema_view,
                                                   struct ArrowError* error,
                                                   const char* ext_name) {
  if (n == 0) {
    if (strcmp(schema->format, "+s") != 0) {
      ArrowErrorSet(error,
                    "Expected storage type struct for coord array for extension '%s'",
                    ext_name);
      return EINVAL;
    }

    if (schema->n_children < 2 || schema->n_children > 4) {
      ArrowErrorSet(
          error,
          "Expected 2, 3, or 4 children for coord array for extension '%s' but got %d",
          ext_name, (int)n);
      return EINVAL;
    }

    char dim[5];
    memset(dim, 0, sizeof(dim));
    for (int64_t i = 0; i < schema->n_children; i++) {
      const char* child_name = schema->children[i]->name;
      if (child_name == NULL || strlen(child_name) != 1) {
        ArrowErrorSet(error,
                      "Expected coordinate child %d to have single character name for "
                      "extension '%s'",
                      (int)i, ext_name);
        return EINVAL;
      }

      if (strcmp(schema->children[i]->format, "g") != 0) {
        ArrowErrorSet(error,
                      "Expected coordinate child %d to have storage type of double for "
                      "extension '%s'",
                      (int)i, ext_name);
        return EINVAL;
      }

      dim[i] = child_name[0];
    }

    if (strcmp(dim, "xy") == 0) {
      schema_view->dimensions = GEOARROW_DIMENSIONS_XY;
    } else if (strcmp(dim, "xyz") == 0) {
      schema_view->dimensions = GEOARROW_DIMENSIONS_XYZ;
    } else if (strcmp(dim, "xym") == 0) {
      schema_view->dimensions = GEOARROW_DIMENSIONS_XYM;
    } else if (strcmp(dim, "xyzm") == 0) {
      schema_view->dimensions = GEOARROW_DIMENSIONS_XYZM;
    } else {
      ArrowErrorSet(error,
                    "Expected dimensions 'xy', 'xyz', 'xym', or 'xyzm' for extension "
                    "'%s' but found '%s'",
                    ext_name, dim);
      return EINVAL;
    }

    schema_view->coord_type = GEOARROW_COORD_TYPE_SEPARATE;
    return GEOARROW_OK;
  } else {
    if (strcmp(schema->format, "+l") != 0 || schema->n_children != 1) {
      ArrowErrorSet(error,
                    "Expected valid list type for coord parent %d for extension '%s'", n,
                    ext_name);
      return EINVAL;
    }

    return GeoArrowParseNestedSchema(schema->children[0], n - 1, schema_view, error,
                                     ext_name);
  }
}

static GeoArrowErrorCode GeoArrowSchemaViewInitInternal(
    struct GeoArrowSchemaView* schema_view, struct ArrowSchema* schema,
    struct ArrowSchemaView* na_schema_view, struct ArrowError* na_error) {
  const char* ext_name = na_schema_view->extension_name.data;
  int64_t ext_len = na_schema_view->extension_name.n_bytes;

  if (ext_len >= 14 && strncmp(ext_name, "geoarrow.point", 14) == 0) {
    schema_view->geometry_type = GEOARROW_GEOMETRY_TYPE_POINT;
    NANOARROW_RETURN_NOT_OK(
        GeoArrowParseNestedSchema(schema, 0, schema_view, na_error, "geoarrow.point"));
    schema_view->type = GeoArrowMakeType(
        schema_view->geometry_type, schema_view->dimensions, schema_view->coord_type);
  } else if (ext_len >= 19 && strncmp(ext_name, "geoarrow.linestring", 19) == 0) {
    schema_view->geometry_type = GEOARROW_GEOMETRY_TYPE_LINESTRING;
    NANOARROW_RETURN_NOT_OK(GeoArrowParseNestedSchema(schema, 1, schema_view, na_error,
                                                      "geoarrow.linestring"));
    schema_view->type = GeoArrowMakeType(
        schema_view->geometry_type, schema_view->dimensions, schema_view->coord_type);
  } else if (ext_len >= 16 && strncmp(ext_name, "geoarrow.polygon", 16) == 0) {
    schema_view->geometry_type = GEOARROW_GEOMETRY_TYPE_POLYGON;
    NANOARROW_RETURN_NOT_OK(
        GeoArrowParseNestedSchema(schema, 2, schema_view, na_error, "geoarrow.polygon"));
    schema_view->type = GeoArrowMakeType(
        schema_view->geometry_type, schema_view->dimensions, schema_view->coord_type);
  } else if (ext_len >= 19 && strncmp(ext_name, "geoarrow.multipoint", 19) == 0) {
    schema_view->geometry_type = GEOARROW_GEOMETRY_TYPE_MULTIPOINT;
    NANOARROW_RETURN_NOT_OK(GeoArrowParseNestedSchema(schema, 1, schema_view, na_error,
                                                      "geoarrow.multipoint"));
    schema_view->type = GeoArrowMakeType(
        schema_view->geometry_type, schema_view->dimensions, schema_view->coord_type);
  } else if (ext_len >= 24 && strncmp(ext_name, "geoarrow.multilinestring", 24) == 0) {
    schema_view->geometry_type = GEOARROW_GEOMETRY_TYPE_MULTILINESTRING;
    NANOARROW_RETURN_NOT_OK(GeoArrowParseNestedSchema(schema, 2, schema_view, na_error,
                                                      "geoarrow.multilinestring"));
    schema_view->type = GeoArrowMakeType(
        schema_view->geometry_type, schema_view->dimensions, schema_view->coord_type);
  } else if (ext_len >= 21 && strncmp(ext_name, "geoarrow.multipolygon", 21) == 0) {
    schema_view->geometry_type = GEOARROW_GEOMETRY_TYPE_MULTIPOLYGON;
    NANOARROW_RETURN_NOT_OK(GeoArrowParseNestedSchema(schema, 3, schema_view, na_error,
                                                      "geoarrow.multipolygon"));
    schema_view->type = GeoArrowMakeType(
        schema_view->geometry_type, schema_view->dimensions, schema_view->coord_type);
  } else if (ext_len >= 12 && strncmp(ext_name, "geoarrow.wkb", 12) == 0) {
    switch (na_schema_view->data_type) {
      case NANOARROW_TYPE_BINARY:
        schema_view->type = GEOARROW_TYPE_WKB;
        break;
      case NANOARROW_TYPE_LARGE_BINARY:
        schema_view->type = GEOARROW_TYPE_LARGE_WKB;
        break;
      default:
        ArrowErrorSet(na_error,
                      "Expected storage type of binary or large_binary for extension "
                      "'geoarrow.wkb'");
        return EINVAL;
    }

    schema_view->geometry_type = GeoArrowGeometryTypeFromType(schema_view->type);
    schema_view->dimensions = GeoArrowDimensionsFromType(schema_view->type);
    schema_view->coord_type = GeoArrowCoordTypeFromType(schema_view->type);
  } else {
    ArrowErrorSet(na_error, "Unrecognized GeoArrow extension name: '%.*s'", (int)ext_len,
                  ext_name);
    return EINVAL;
  }

  schema_view->extension_name.data = na_schema_view->extension_name.data;
  schema_view->extension_name.n_bytes = na_schema_view->extension_name.n_bytes;
  schema_view->extension_metadata.data = na_schema_view->extension_metadata.data;
  schema_view->extension_metadata.n_bytes = na_schema_view->extension_metadata.n_bytes;

  return GEOARROW_OK;
}

GeoArrowErrorCode GeoArrowSchemaViewInit(struct GeoArrowSchemaView* schema_view,
                                         struct ArrowSchema* schema,
                                         struct GeoArrowError* error) {
  struct ArrowError* na_error = (struct ArrowError*)error;
  struct ArrowSchemaView na_schema_view;
  NANOARROW_RETURN_NOT_OK(ArrowSchemaViewInit(&na_schema_view, schema, na_error));

  const char* ext_name = na_schema_view.extension_name.data;
  int64_t ext_len = na_schema_view.extension_name.n_bytes;

  if (ext_name == NULL) {
    ArrowErrorSet(na_error, "Expected extension type");
    return EINVAL;
  }

  return GeoArrowSchemaViewInitInternal(schema_view, schema, &na_schema_view, na_error);
}

GeoArrowErrorCode GeoArrowSchemaViewInitFromExtensionName(
    struct GeoArrowSchemaView* schema_view, struct ArrowSchema* schema,
    struct GeoArrowStringView extension_name, struct GeoArrowError* error) {
  struct ArrowError* na_error = (struct ArrowError*)error;
  struct ArrowSchemaView na_schema_view;
  NANOARROW_RETURN_NOT_OK(ArrowSchemaViewInit(&na_schema_view, schema, na_error));
  na_schema_view.extension_name.data = extension_name.data;
  na_schema_view.extension_name.n_bytes = extension_name.n_bytes;
  return GeoArrowSchemaViewInitInternal(schema_view, schema, &na_schema_view, na_error);
}

GeoArrowErrorCode GeoArrowSchemaViewInitFromType(struct GeoArrowSchemaView* schema_view,
                                                 enum GeoArrowType type) {
  schema_view->schema = NULL;
  schema_view->extension_name.data = NULL;
  schema_view->extension_name.n_bytes = 0;
  schema_view->extension_metadata.data = NULL;
  schema_view->extension_metadata.n_bytes = 0;
  schema_view->type = type;
  schema_view->geometry_type = GeoArrowGeometryTypeFromType(type);
  schema_view->dimensions = GeoArrowDimensionsFromType(type);
  schema_view->coord_type = GeoArrowCoordTypeFromType(type);

  if (type == GEOARROW_TYPE_UNINITIALIZED) {
    return GEOARROW_OK;
  }

  const char* extension_name = GeoArrowExtensionNameFromType(type);
  if (extension_name == NULL) {
    return EINVAL;
  }

  schema_view->extension_name.data = extension_name;
  schema_view->extension_name.n_bytes = strlen(extension_name);

  return GEOARROW_OK;
}
