#include "graph_io_aux.h"

static void *Realloc(void *ptr, size_t sz) {

  void *lp;

  lp = (void *) realloc(ptr, sz);
  if (!lp && sz) {
    fprintf(stderr, "Cannot reallocate to %zu bytes...\n", sz);
    exit(EXIT_FAILURE);
  }
  return lp;
}

static FILE *Fopen(const char *path, const char *mode) {

  FILE *fp = NULL;
  fp = fopen(path, mode);
  if (!fp) {
    fprintf(stderr, "Cannot open file %s...\n", path);
    exit(EXIT_FAILURE);
  }
  return fp;
}

static uint64_t getFsize(FILE *fp) {

  int64_t rv;
  uint64_t size = 0;

  rv = fseek(fp, 0, SEEK_END);
  if (rv != 0) {
    fprintf(stderr, "SEEK END FAILED\n");
    if (ferror(fp)) fprintf(stderr, "FERROR SET\n");
    exit(EXIT_FAILURE);
  }

  size = ftell(fp);
  rv = fseek(fp, 0, SEEK_SET);

  if (rv != 0) {
    fprintf(stderr, "SEEK SET FAILED\n");
    exit(EXIT_FAILURE);
  }

  return size;
}

template <typename dtype>
const char * get_fmt<dtype>(){
  printf("CTF ERROR: Format of tensor unsupported for sparse I/O\n");
  IASSERT(0);
}

template <>
const char * get_fmt<float>(){
  return " %f";
}

template <>
const char * get_fmt<double>(){
  return " %lf";
}

template <>
const char * get_fmt<int>(){
  return " %d";
}

template <>
const char * get_fmt<int64_t>(){
  return " %ld";
}

template <typename dtype>
void process_tensor(char **lvals, int order, int *lens, uint64_t nvals, int64_t **inds, dtype **vals) {
  int64_t i = 0;
  *inds=(int64_t *)malloc(nvals*sizeof(int64_t));
  *vals=(int64_t *)malloc(nvals*sizeof(dtype));

  int64_t * ind = (int64_t *)malloc(order*sizeof(int64_t));
  char * str = (char *)malloc(sizeof(char)*(order*4+4));
  strcpy(str, "%ld");
  for (i=0; i<order-1; i++){
    strcat(str, " %ld");
  }
  strcat(str, get_fmt<dtype>());
  for (i=0; i<nvals; i++) {
    double v;
    // TODO: build string of the form %lu ... order times .... %lu <symb>
    // define symb based on dtype
    switch (order){
      case 1:
        sscanf(lvals[i], str, ind+0, &v);
        break;
      case 2:
        sscanf(lvals[i], str, ind+0, ind+1, &v);
        break;
      case 3:
        sscanf(lvals[i], str, ind+0, ind+1, ind+2, &v);
        break;
      case 4:
        sscanf(lvals[i], str, ind+0, ind+1, ind+2, ind+3, &v);
        break;
      case 5:
        sscanf(lvals[i], str, ind+0, ind+1, ind+2, ind+3, ind+4, &v);
        break;
      case 6:
        sscanf(lvals[i], str, ind+0, ind+1, ind+2, ind+3, ind+4, ind+5, &v);
        break;
      case 7:
        sscanf(lvals[i], str, ind+0, ind+1, ind+2, ind+3, ind+4, ind+5, ind+6, &v);
        break;
      case 8:
        sscanf(lvals[i], str, ind+0, ind+1, ind+2, ind+3, ind+4, ind+5, ind+6, ind+7, &v);
        break;
      case 9:
        sscanf(lvals[i], str, ind+0, ind+1, ind+2, ind+3, ind+4, ind+5, ind+6, ind+7, ind+8, &v);
        break;
      case 10:
        sscanf(lvals[i], str, ind+0, ind+1, ind+2, ind+3, ind+4, ind+5, ind+6, ind+7, ind+8, ind+9, &v);
        break;

      default:
        printf("CTF ERROR: this tensor order not supported for sparse I/O\n");
        break;
    }
    int64_t lda = 1;
    for (int j=0; j<order; j++){
      (*inds)[i] += ind[j]*lda;
      lda *= lens[j];
    }
    (*vals)[i] = v;
  }
  free(ind);
  free(str);
}

template <typename dtype>
uint64_t read_data_mpiio(int myid, int ntask, const char *fpath, char ***led){
  MPI_File fh;
  MPI_Offset filesize;
  MPI_Offset localsize;
  MPI_Offset start,end;
  MPI_Status status;
  char *chunk = NULL;
  int MPI_RESULT = 0;
  int overlap = 100; // define
  int64_t ned = 0;
  int64_t i = 0;

  MPI_RESULT = MPI_File_open(MPI_COMM_WORLD,fpath, MPI_MODE_RDONLY, MPI_INFO_NULL,&fh);

  /* Get the size of file */
  MPI_File_get_size(fh, &filesize); //return in bytes
  // FIXME: skewed to give lower processor counts more vals, since
  //    smaller node counts contain fewer characters
  localsize = filesize/ntask;
  start = myid * localsize;
  end = start + localsize;
  end +=overlap;

  if (myid  == ntask-1) end = filesize;
  localsize = end - start; //OK

  chunk = (char*)malloc( (localsize + 1)*sizeof(char));
  MPI_File_read_at_all(fh, start, chunk, localsize, MPI_CHAR, &status);
  chunk[localsize] = '\0';

  int64_t locstart=0, locend=localsize;
  if (myid != 0) {
    while(chunk[locstart] != '\n') locstart++;
    locstart++;
  }
  if (myid != ntask-1) {
    locend-=overlap;
    while(chunk[locend] != '\n') locend++;
    locend++;
  }
  localsize = locend-locstart; //OK

  char *data = (char *)malloc((localsize+1)*sizeof(char));
  memcpy(data, &(chunk[locstart]), localsize);
  data[localsize] = '\0';
  free(chunk);

  //printf("[%d] local chunk = [%ld,%ld) / %ld\n", myid, start+locstart, start+locstart+localsize, filesize);
  for ( i=0; i<localsize; i++){
    if (data[i] == '\n') ned++;
  }
  //printf("[%d] ned= %ld\n",myid, ned);

  (*led) = (char **)malloc(ned*sizeof(char *));
  (*led)[0] = strtok(data,"\n");

  for ( i=1; i < ned; i++)
    (*led)[i] = strtok(NULL, "\n");

  MPI_File_close(&fh);

  return ned;
}

