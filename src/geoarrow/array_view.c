
#include <errno.h>

#include "geoarrow.h"

#include "nanoarrow.h"

static int32_t kZeroInt32 = 0;

static int GeoArrowArrayViewInitInternal(struct GeoArrowArrayView* array_view,
                                         struct GeoArrowError* error) {
  switch (array_view->schema_view.geometry_type) {
    case GEOARROW_GEOMETRY_TYPE_POINT:
      array_view->n_offsets = 0;
      break;
    case GEOARROW_GEOMETRY_TYPE_LINESTRING:
    case GEOARROW_GEOMETRY_TYPE_MULTIPOINT:
      array_view->n_offsets = 1;
      break;
    case GEOARROW_GEOMETRY_TYPE_POLYGON:
    case GEOARROW_GEOMETRY_TYPE_MULTILINESTRING:
      array_view->n_offsets = 2;
      break;
    case GEOARROW_GEOMETRY_TYPE_MULTIPOLYGON:
      array_view->n_offsets = 3;
      break;
    default:
      ArrowErrorSet((struct ArrowError*)error,
                    "Unsupported geometry type in GeoArrowArrayViewInit()");
      return EINVAL;
  }

  array_view->length = 0;
  array_view->validity_bitmap = NULL;
  for (int i = 0; i < 4; i++) {
    array_view->offsets[i] = NULL;
  }

  array_view->coords.n_coords = 0;
  switch (array_view->schema_view.dimensions) {
    case GEOARROW_DIMENSIONS_XY:
      array_view->coords.n_values = 2;
      break;
    case GEOARROW_DIMENSIONS_XYZ:
    case GEOARROW_DIMENSIONS_XYM:
      array_view->coords.n_values = 3;
      break;
    case GEOARROW_DIMENSIONS_XYZM:
      array_view->coords.n_values = 4;
      break;
    default:
      ArrowErrorSet((struct ArrowError*)error,
                    "Unsupported dimensions in GeoArrowArrayViewInit()");
      return EINVAL;
  }

  switch (array_view->schema_view.coord_type) {
    case GEOARROW_COORD_TYPE_SEPARATE:
      array_view->coords.coords_stride = 1;
      break;
    case GEOARROW_COORD_TYPE_INTERLEAVED:
      array_view->coords.coords_stride = array_view->coords.n_values;
      break;
    default:
      ArrowErrorSet((struct ArrowError*)error,
                    "Unsupported coord type in GeoArrowArrayViewInit()");
      return EINVAL;
  }

  for (int i = 0; i < 4; i++) {
    array_view->coords.values[i] = NULL;
  }

  return GEOARROW_OK;
}

GeoArrowErrorCode GeoArrowArrayViewInitFromType(struct GeoArrowArrayView* array_view,
                                                enum GeoArrowType type) {
  NANOARROW_RETURN_NOT_OK(GeoArrowSchemaViewInitFromType(&array_view->schema_view, type));
  return GeoArrowArrayViewInitInternal(array_view, NULL);
}

GeoArrowErrorCode GeoArrowArrayViewInitFromSchema(struct GeoArrowArrayView* array_view,
                                                  struct ArrowSchema* schema,
                                                  struct GeoArrowError* error) {
  NANOARROW_RETURN_NOT_OK(
      GeoArrowSchemaViewInit(&array_view->schema_view, schema, error));
  return GeoArrowArrayViewInitInternal(array_view, error);
}

static int GeoArrowArrayViewSetArrayInternal(struct GeoArrowArrayView* array_view,
                                             struct ArrowArray* array,
                                             struct GeoArrowError* error, int level) {
  if (array->offset != 0) {
    // This should be supported at some point
    ArrowErrorSet((struct ArrowError*)error,
                  "ArrowArray with offset != 0 is not yet supported in "
                  "GeoArrowArrayViewSetArray()");
    return ENOTSUP;
  }

  if (level == array_view->n_offsets) {
    // We're at the coord array!

    // n_coords is last_offset[level - 1] or array->length if level == 0
    if (level > 0) {
      array_view->coords.n_coords = array_view->last_offset[level - 1];
    } else {
      array_view->coords.n_coords = array->length;
    }

    switch (array_view->schema_view.coord_type) {
      case GEOARROW_COORD_TYPE_SEPARATE:
        if (array->n_children != array_view->coords.n_values) {
          ArrowErrorSet((struct ArrowError*)error,
                        "Unexpected number of children for struct coordinate array "
                        "in GeoArrowArrayViewSetArray()");
          return EINVAL;
        }

        // Set the coord pointers to the data buffer of each child
        for (int32_t i = 0; i < array_view->coords.n_values; i++) {
          if (array->children[i]->n_buffers != 2) {
            ArrowErrorSet(
                (struct ArrowError*)error,
                "Unexpected number of buffers for struct coordinate array child "
                "in GeoArrowArrayViewSetArray()");
            return EINVAL;
          }

          array_view->coords.values[i] = ((const double*)array->children[i]->buffers[1]);
        }

        break;

      case GEOARROW_COORD_TYPE_INTERLEAVED:
        if (array->n_children != 1) {
          ArrowErrorSet((struct ArrowError*)error,
                        "Unexpected number of children for interleaved coordinate array "
                        "in GeoArrowArrayViewSetArray()");
          return EINVAL;
        }

        if (array->children[0]->n_buffers != 2) {
          ArrowErrorSet(
              (struct ArrowError*)error,
              "Unexpected number of buffers for interleaved coordinate array child "
              "in GeoArrowArrayViewSetArray()");
          return EINVAL;
        }

        // Set the coord pointers to the first four doubles in the data buffers
        for (int32_t i = 0; i < array_view->coords.n_values; i++) {
          array_view->coords.values[i] =
              ((const double*)array->children[0]->buffers[1]) + i;
        }

        break;

      default:
        ArrowErrorSet((struct ArrowError*)error,
                      "Unexpected coordinate type GeoArrowArrayViewSetArray()");
        return EINVAL;
    }

    return GEOARROW_OK;
  }

  if (array->n_buffers != 2) {
    ArrowErrorSet(
        (struct ArrowError*)error,
        "Unexpected number of buffers in list array in GeoArrowArrayViewSetArray()");
    return EINVAL;
  }

  if (array->n_children != 1) {
    ArrowErrorSet(
        (struct ArrowError*)error,
        "Unexpected number of children in list array in GeoArrowArrayViewSetArray()");
    return EINVAL;
  }

  // Set the offsets buffer and the last_offset value of level
  if (array->length > 0) {
    array_view->offsets[level] = (const int32_t*)array->buffers[1];
    array_view->last_offset[level] =
        array_view->offsets[level][array->offset + array->length];
  } else {
    array_view->offsets[level] = &kZeroInt32;
    array_view->last_offset[level] = 0;
  }

  return GeoArrowArrayViewSetArrayInternal(array_view, array->children[0], error,
                                           level + 1);
}

GeoArrowErrorCode GeoArrowArrayViewSetArray(struct GeoArrowArrayView* array_view,
                                            struct ArrowArray* array,
                                            struct GeoArrowError* error) {
  NANOARROW_RETURN_NOT_OK(GeoArrowArrayViewSetArrayInternal(array_view, array, error, 0));
  array_view->validity_bitmap = array->buffers[0];
  array_view->length = array->length;
  return GEOARROW_OK;
}

static inline void GeoArrowCoordViewUpdate(struct GeoArrowCoordView* src,
                                           struct GeoArrowCoordView* dst, int64_t offset,
                                           int64_t length) {
  for (int j = 0; j < dst->n_values; j++) {
    dst->values[j] = src->values[j] + (offset * src->coords_stride);
  }
  dst->n_coords = length;
}

static GeoArrowErrorCode GeoArrowArrayViewVisitPoint(struct GeoArrowArrayView* array_view,
                                                     int64_t offset, int64_t length,
                                                     struct GeoArrowVisitor* v) {
  struct GeoArrowCoordView coords = array_view->coords;

  for (int64_t i = 0; i < length; i++) {
    NANOARROW_RETURN_NOT_OK(v->feat_start(v));
    if (!array_view->validity_bitmap ||
        ArrowBitGet(array_view->validity_bitmap, offset + i)) {
      NANOARROW_RETURN_NOT_OK(v->geom_start(v, GEOARROW_GEOMETRY_TYPE_POINT,
                                            array_view->schema_view.dimensions));
      GeoArrowCoordViewUpdate(&array_view->coords, &coords, offset + i, 1);
      NANOARROW_RETURN_NOT_OK(v->coords(v, &coords));
      NANOARROW_RETURN_NOT_OK(v->geom_end(v));
    } else {
      NANOARROW_RETURN_NOT_OK(v->null_feat(v));
    }

    NANOARROW_RETURN_NOT_OK(v->feat_end(v));

    for (int j = 0; j < coords.n_values; j++) {
      coords.values[j] += coords.coords_stride;
    }
  }

  return GEOARROW_OK;
}

static GeoArrowErrorCode GeoArrowArrayViewVisitLinestring(
    struct GeoArrowArrayView* array_view, int64_t offset, int64_t length,
    struct GeoArrowVisitor* v) {
  struct GeoArrowCoordView coords = array_view->coords;

  int64_t coord_offset;
  int64_t n_coords;
  for (int64_t i = 0; i < length; i++) {
    NANOARROW_RETURN_NOT_OK(v->feat_start(v));
    if (!array_view->validity_bitmap ||
        ArrowBitGet(array_view->validity_bitmap, offset + i)) {
      NANOARROW_RETURN_NOT_OK(v->geom_start(v, GEOARROW_GEOMETRY_TYPE_LINESTRING,
                                            array_view->schema_view.dimensions));
      coord_offset = array_view->offsets[0][offset + i];
      n_coords = array_view->offsets[0][offset + i + 1] - coord_offset;
      GeoArrowCoordViewUpdate(&array_view->coords, &coords, coord_offset, n_coords);
      NANOARROW_RETURN_NOT_OK(v->coords(v, &coords));
      NANOARROW_RETURN_NOT_OK(v->geom_end(v));
    } else {
      NANOARROW_RETURN_NOT_OK(v->null_feat(v));
    }

    NANOARROW_RETURN_NOT_OK(v->feat_end(v));
  }

  return GEOARROW_OK;
}

static GeoArrowErrorCode GeoArrowArrayViewVisitPolygon(
    struct GeoArrowArrayView* array_view, int64_t offset, int64_t length,
    struct GeoArrowVisitor* v) {
  struct GeoArrowCoordView coords = array_view->coords;

  int64_t ring_offset;
  int64_t n_rings;
  int64_t coord_offset;
  int64_t n_coords;
  for (int64_t i = 0; i < length; i++) {
    NANOARROW_RETURN_NOT_OK(v->feat_start(v));
    if (!array_view->validity_bitmap ||
        ArrowBitGet(array_view->validity_bitmap, offset + i)) {
      NANOARROW_RETURN_NOT_OK(v->geom_start(v, GEOARROW_GEOMETRY_TYPE_POLYGON,
                                            array_view->schema_view.dimensions));
      ring_offset = array_view->offsets[0][offset + i];
      n_rings = array_view->offsets[0][offset + i + 1] - ring_offset;

      for (int64_t j = 0; j < n_rings; j++) {
        NANOARROW_RETURN_NOT_OK(v->ring_start(v));
        coord_offset = array_view->offsets[1][ring_offset + j];
        n_coords = array_view->offsets[1][ring_offset + j + 1] - coord_offset;
        GeoArrowCoordViewUpdate(&array_view->coords, &coords, coord_offset, n_coords);
        NANOARROW_RETURN_NOT_OK(v->coords(v, &coords));
        NANOARROW_RETURN_NOT_OK(v->ring_end(v));
      }

      NANOARROW_RETURN_NOT_OK(v->geom_end(v));
    } else {
      NANOARROW_RETURN_NOT_OK(v->null_feat(v));
    }

    NANOARROW_RETURN_NOT_OK(v->feat_end(v));
  }

  return GEOARROW_OK;
}

static GeoArrowErrorCode GeoArrowArrayViewVisitMultipoint(
    struct GeoArrowArrayView* array_view, int64_t offset, int64_t length,
    struct GeoArrowVisitor* v) {
  struct GeoArrowCoordView coords = array_view->coords;

  int64_t coord_offset;
  int64_t n_coords;
  for (int64_t i = 0; i < length; i++) {
    NANOARROW_RETURN_NOT_OK(v->feat_start(v));
    if (!array_view->validity_bitmap ||
        ArrowBitGet(array_view->validity_bitmap, offset + i)) {
      NANOARROW_RETURN_NOT_OK(v->geom_start(v, GEOARROW_GEOMETRY_TYPE_MULTIPOINT,
                                            array_view->schema_view.dimensions));
      coord_offset = array_view->offsets[0][offset + i];
      n_coords = array_view->offsets[0][offset + i + 1] - coord_offset;
      for (int64_t j = 0; j < n_coords; j++) {
        NANOARROW_RETURN_NOT_OK(v->geom_start(v, GEOARROW_GEOMETRY_TYPE_POINT,
                                              array_view->schema_view.dimensions));
        GeoArrowCoordViewUpdate(&array_view->coords, &coords, coord_offset + j, 1);
        NANOARROW_RETURN_NOT_OK(v->coords(v, &coords));
        NANOARROW_RETURN_NOT_OK(v->geom_end(v));
      }
      NANOARROW_RETURN_NOT_OK(v->geom_end(v));
    } else {
      NANOARROW_RETURN_NOT_OK(v->null_feat(v));
    }

    NANOARROW_RETURN_NOT_OK(v->feat_end(v));
  }

  return GEOARROW_OK;
}

static GeoArrowErrorCode GeoArrowArrayViewVisitMultilinestring(
    struct GeoArrowArrayView* array_view, int64_t offset, int64_t length,
    struct GeoArrowVisitor* v) {
  struct GeoArrowCoordView coords = array_view->coords;

  int64_t linestring_offset;
  int64_t n_linestrings;
  int64_t coord_offset;
  int64_t n_coords;
  for (int64_t i = 0; i < length; i++) {
    NANOARROW_RETURN_NOT_OK(v->feat_start(v));
    if (!array_view->validity_bitmap ||
        ArrowBitGet(array_view->validity_bitmap, offset + i)) {
      NANOARROW_RETURN_NOT_OK(v->geom_start(v, GEOARROW_GEOMETRY_TYPE_MULTILINESTRING,
                                            array_view->schema_view.dimensions));
      linestring_offset = array_view->offsets[0][offset + i];
      n_linestrings = array_view->offsets[0][offset + i + 1] - linestring_offset;

      for (int64_t j = 0; j < n_linestrings; j++) {
        NANOARROW_RETURN_NOT_OK(v->geom_start(v, GEOARROW_GEOMETRY_TYPE_LINESTRING,
                                              array_view->schema_view.dimensions));
        coord_offset = array_view->offsets[1][linestring_offset + j];
        n_coords = array_view->offsets[1][linestring_offset + j + 1] - coord_offset;
        GeoArrowCoordViewUpdate(&array_view->coords, &coords, coord_offset, n_coords);
        NANOARROW_RETURN_NOT_OK(v->coords(v, &coords));
        NANOARROW_RETURN_NOT_OK(v->geom_end(v));
      }

      NANOARROW_RETURN_NOT_OK(v->geom_end(v));
    } else {
      NANOARROW_RETURN_NOT_OK(v->null_feat(v));
    }

    NANOARROW_RETURN_NOT_OK(v->feat_end(v));
  }

  return GEOARROW_OK;
}

GeoArrowErrorCode GeoArrowArrayViewVisit(struct GeoArrowArrayView* array_view,
                                         int64_t offset, int64_t length,
                                         struct GeoArrowVisitor* v) {
  switch (array_view->schema_view.geometry_type) {
    case GEOARROW_GEOMETRY_TYPE_POINT:
      return GeoArrowArrayViewVisitPoint(array_view, offset, length, v);
    case GEOARROW_GEOMETRY_TYPE_LINESTRING:
      return GeoArrowArrayViewVisitLinestring(array_view, offset, length, v);
    case GEOARROW_GEOMETRY_TYPE_POLYGON:
      return GeoArrowArrayViewVisitPolygon(array_view, offset, length, v);
    case GEOARROW_GEOMETRY_TYPE_MULTIPOINT:
      return GeoArrowArrayViewVisitMultipoint(array_view, offset, length, v);
    case GEOARROW_GEOMETRY_TYPE_MULTILINESTRING:
      return GeoArrowArrayViewVisitMultilinestring(array_view, offset, length, v);
    default:
      return ENOTSUP;
  }
}
