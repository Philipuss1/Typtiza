#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL_image.h>
#include <GL/gl.h>

#include <math.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include <dirent.h>


#define print(fmt, ...) printf (fmt, ##__VA_ARGS__); fflush (stdout)
#define FOR(count) for (int i = 0; i < count; ++i)


char myname[32];

struct Msg {
  char *text;
  char *name;
};

int  messages_max = 64;
int  messages_count = 0;
Msg *messages;


SOCKET my_socket = -1;
int window_width  = 480 * 1;
int window_height = 480 * 1;

struct Texture {
  int w, h;
  GLuint id;
};


struct Entity {
  float x;
  float y;
  float tx;
  float ty;
  float tw;
  float th;
  float frame;
};


Texture global_texture;

static float
rand32 (void)
{
  return rand () / (float) (RAND_MAX + 1ULL);
}

static Texture
load_image (const char *filepath)
{
  printf("%s\n", filepath);
  SDL_Surface *image = IMG_Load(filepath);
  assert(image);

  Texture texture;
  texture.w = image->w;
  texture.h = image->h;
  glGenTextures (1, &texture.id);
  glBindTexture (GL_TEXTURE_2D, texture.id);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8,
                texture.w, texture.h,
                0,
                GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);

  return texture;
}


void draw_rect (float x, float y, float w, float h)
{
  glBegin (GL_TRIANGLE_STRIP);
  glVertex2f (x    , y - h);
  glVertex2f (x    , y    );
  glVertex2f (x + w, y - h);
  glVertex2f (x + w, y    );
  glEnd ();
}


void draw_image (float x, float y, float tx, float ty, float tw, float th)
{
  float tx0 = tx / global_texture.w;
  float tx1 = (tx + tw) / global_texture.w;
  float ty0 = ty / global_texture.h;
  float ty1 = (ty + th) / global_texture.h;

  glEnable (GL_TEXTURE_2D);

  float w = tw / 2;
  float h = th / 2;

  glBegin (GL_TRIANGLE_STRIP);
  glTexCoord2f (tx0, ty1);
  glVertex2f (x - w, y - h);
  glTexCoord2f (tx0, ty0);
  glVertex2f (x - w, y + h);
  glTexCoord2f (tx1, ty1);
  glVertex2f (x + w, y - h);
  glTexCoord2f (tx1, ty0);
  glVertex2f (x + w, y + h);
  glEnd ();

  glDisable (GL_TEXTURE_2D);
}

void draw_entity (Entity entity)
{
  float tx = entity.tx + (int) entity.frame * entity.tw;
  draw_image (entity.x, entity.y, tx, entity.ty, entity.tw, entity.th);
}

void draw_text (float x, float y, const char *text, float scale=1)
{
  // float matrix[16] = {
  //   4.0 / window_width, 0.00, 0.00, 0.00,
  //   0.00, 4.0 / window_height, 0.00, 0.00,
  //   0.00, 0.00, 1.00, 0.00,
  //   0.00, 0.00, 0.00, 1.00,
  // };
  // glLoadMatrixf (matrix);

  glEnable (GL_TEXTURE_2D);

  float glyph_w = 5.0f * scale;
  float glyph_h = 10.0f * scale;

  float ty0 = (float) (global_texture.h - glyph_h) / global_texture.h;
  float ty1 = 1;
  
  size_t text_len = strlen (text);
  glColor3f (1, 1, 1);

  for (size_t i = 0; i < text_len; ++i)
    {
      int glyph_num = text[i] - 32;

      float tx0 = (0 + glyph_num * 6) / (float) global_texture.w;
      float tx1 = (5 + glyph_num * 6) / (float) global_texture.w;
      
      glBegin (GL_TRIANGLE_STRIP);
      glTexCoord2f (tx0, ty1);
      glVertex2f (x          , y);
      glTexCoord2f (tx0, ty0);
      glVertex2f (x          , y + glyph_h);
      glTexCoord2f (tx1, ty1);
      glVertex2f (x + glyph_w, y);
      glTexCoord2f (tx1, ty0);
      glVertex2f (x + glyph_w, y + glyph_h);
      glEnd ();

      x += 6 * scale;
    }

  glDisable (GL_TEXTURE_2D);
}

void draw_type_space (float x, float y)
{
  draw_image (x, y, 28, 0, 244, 11);
}

void draw_add_button (float x, float y)
{
  draw_image (x, y, 49, 989, 15, 15);
}

void draw_emojy_icon (float x, float y)
{
  draw_image (x, y, 0, 989, 15, 15);
}

void draw_call_button (float x, float y)
{
  draw_image (x, y, 16, 989, 15, 15);
}

void draw_record_button (float x, float y)
{
  draw_image (x, y,33, 989, 15, 15);
}

static Msg
make_message (const char *text, const char *name)
{
  Msg msg;
  
  size_t text_len = strlen (text);
  msg.text = (char *) malloc (text_len + 1);
  memcpy (msg.text, text, text_len + 1);
  
  if (name)
    {
      size_t name_len = strlen (name);
      msg.name = (char *) malloc (name_len + 1);
      memcpy (msg.name, name, name_len + 1);
    }
  else
    {
      msg.name = NULL;
    }
  
  return msg;
}


DWORD WINAPI get_messages (LPVOID param)
{
  while (1)
    {
      //if (current_message >= 20) current_message = 1;
      char buffer[4096];
      int message_len = recv (my_socket, buffer, sizeof(buffer)-1, 0);
      buffer[message_len] = 0;

      char *name = buffer;
      char *text = strchr (buffer, ':');

      if (text)
        {
          text[0] = 0;
          text++;
          messages[messages_count++] = make_message (text, name);
        }
    }
  return 0;
}


void
connect_to (const char* host)
{
  WSADATA winsock;
  int winsock_error = WSAStartup (MAKEWORD (2, 2), &winsock);
  assert (!winsock_error);

  addrinfo hints = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  addrinfo *addr = NULL;
  int getaddrinfo_error = getaddrinfo (host, "3141", &hints, &addr);
  assert (getaddrinfo_error != -1);
  assert (addr);

  my_socket = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
  assert (my_socket != INVALID_SOCKET);

  int connect_error = connect (my_socket, addr->ai_addr, sizeof (sockaddr_in));
  if (!connect_error)
    {
      printf ("Connected\n");
      fflush (stdout);
      HANDLE net_thread = CreateThread (NULL, 0, get_messages, NULL, 0, NULL);
      assert (net_thread);
    }
  else
    {
      printf ("Not online\n");
      fflush (stdout);
    }
}




void send_file (const char *file_path)
{
  FILE *file = fopen (file_path, "rb");
  if (file)
    {
      fseek (file, 0, SEEK_END);
      long file_length = ftell (file);
      rewind (file);

      char *file_data = (char *) malloc (file_length);
      fread (file_data, 1, file_length, file);
      fclose (file);
                          
      int bytes_sent = send (my_socket, file_data, file_length, 0);
      if (bytes_sent == SOCKET_ERROR)
        {
          printf ("ERROR!");
        }
      free (file_data);
    }
  else
    {
      // strcpy (messages[current_message++], "File doesn't exist");
    }
}

int input_x = 0;
int input_y = 0;
int input_rmb = 0;


struct FileEntry {
  char name[256];
  int is_dir;
};


FileEntry file_entries[4096];
int       file_entries_count = 0;
int       current_file = 0;
char      current_filepath[4096] = ".";


void list_dir (const char *new_dirpath=0)
{
  const char *dirpath;

  if (new_dirpath)
    {
      dirpath = new_dirpath;
    }
  else
    {
      dirpath = current_filepath;
    }

  file_entries_count = 0;
  current_file = 0;

  size_t path_len = strlen (dirpath);
  size_t file_search_path_len = path_len + 2;
  char file_search_path[file_search_path_len + 1];

  strcpy (file_search_path, dirpath);
  strcat (file_search_path, "\\*");

  printf ("file_search_path: %s\n", file_search_path);
  fflush (stdout);

  WIN32_FIND_DATAA entry;
  HANDLE dir = FindFirstFileA (file_search_path, &entry);
  if (dir != INVALID_HANDLE_VALUE)
    {
      do
        {
          if (!strcmp (entry.cFileName, ".")) continue;

          FileEntry file_entry;
          strcpy (file_entry.name, entry.cFileName);
          // printf ("file: %s\n", file_entry.name);
          // fflush (stdout);
          file_entry.is_dir = !!(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
          //file_entry.is_dir = entry->d_type == DT_DIR;

          file_entries[file_entries_count++] = file_entry;
        }
      while (FindNextFileA (dir, &entry));
    }
  else
    {
      printf ("Not working!\n");
      fflush (stdout);
    }
  FindClose (dir);
}

void
open_current_file ()
{
  FileEntry file_entry = file_entries[current_file];
  strcat (current_filepath, "\\");
  strcat (current_filepath, file_entry.name);
                      
  if (file_entry.is_dir)
    {
      list_dir ();
    }
  else
    {
      send_file (current_filepath);
      strcpy (current_filepath, ".");
      file_entries_count = 0;
      current_file = 0;
    }
}



  
static void
draw_left_message (Msg msg, int y)
{
  float x = window_width  / -4 + 6;
  glColor3f (1, 1, 1);

  draw_image (x, y - 8, 14, 0, 6, 16);
  x += 6;

  size_t text_len = strlen (msg.text);
  for (size_t i = 1; i < text_len; ++i)
    {
      draw_image (x, y - 7, 21, 0, 6, 14);
      x += 6;
    }
  draw_image (x, y - 7,  22, 0,  6, 14);

  x = window_width / -4 + 7;

    float matrix2[16] = {
                      2.0f / window_width, 0.00, 0.00, 0.00,
                      0.00, 2.0f / window_height, 0.00, 0.00,
                      0.00, 0.00, 1.00, 0.00,
                      0.00, 0.00, 0.00, 1.00,
  };
  glLoadMatrixf (matrix2);
  
  draw_text (x * 2, y * 2, msg.name);
  
  y = y - 11;
  float matrix[16] = {
                      4.0f / window_width, 0.00, 0, 0.00,
                      0.00, 4.0f / window_height, 0.00, 0.00,
                      x, (float)y, 1.00, 0.00,
                      0.00, 0.00, 0.00, 1.00,
  };
  glLoadMatrixf (matrix);
  
  draw_text (x, y, msg.text);
}


static void
draw_right_message (Msg msg, int y)
{
  size_t text_len = strlen (msg.text);
  float x = window_width  / 4 - text_len * 6 - 6;
  glColor3f (1, 1, 1);
  
  draw_image (x, y - 7, 0, 0, 6, 14);
  x += 6;
 
  for (size_t i = 1; i < text_len; ++i)
    {
      draw_image (x, y - 7, 1, 0, 6, 14);
      x += 6;
    }
  
  draw_image (x, y - 8, 8, 0,  6, 16);
  
  x = window_width  / 4 - text_len * 6 - 5;

  
  float matrix2[16] = {
                      2.0f / window_width, 0.00, 0.00, 0.00,
                      0.00, 2.0f / window_height, 0.00, 0.00,
                      0.00, 0.00, 1.00, 0.00,
                      0.00, 0.00, 0.00, 1.00,
  };
  glLoadMatrixf (matrix2);
  
  draw_text (x * 2, y * 2, myname);

  y = y-11;
  
 float matrix[16] = {
                      4.0f / window_width, 0.00, 0, 0.00,
                      0.00, 4.0f / window_height, 0.00, 0.00,
                      x, (float)y, 1.00, 0.00,
                      0.00, 0.00, 0.00, 1.00,
  };
  glLoadMatrixf (matrix);
  
  draw_text (x, y, msg.text);
}


#include <string>
#include <mmsystem.h>
class CMP3_MCI
{
public:
  CMP3_MCI(){m_bPaused = false;}
  ~CMP3_MCI(){Unload();}

  inline void Load(char *szFileName)
  {
    m_szFileName = szFileName;
    Load();
  }

  inline void Load(std::string szFileName)
  {
    m_szFileName = szFileName;
    Load();
  }

  inline void Play()
  {
    std::string szCommand = "play " + GetFileName() + " from 0";
    mciSendString(szCommand.c_str(), NULL, 0, 0);
  }

  inline void Stop()
  {
    std::string szCommand = "stop " + GetFileName();
    mciSendString(szCommand.c_str(), NULL, 0, 0);
  }

  inline void Pause()
  {
    std::string szCommand;
    if(IsPaused())
      {
        szCommand = "resume " + GetFileName();
        mciSendString(szCommand.c_str(), NULL, 0, 0);
        SetPaused(false);
      }
    else
      {
        szCommand = "pause " + GetFileName();
        mciSendString(szCommand.c_str(), NULL, 0, 0);		
        SetPaused(true);
      }
  }

  inline void Unload()
  {
    std::string szCommand = "close" + GetFileName();
    Stop();
    mciSendString(szCommand.c_str(), NULL, 0, 0);
  }

  //Accessor's for private members.
  inline std::string GetFileName()
  {return m_szFileName;}

  inline bool IsPaused()
  {return m_bPaused;}

  inline void SetPaused(bool bPaused)
  {m_bPaused = bPaused;}
private:
  inline void Load()
  {
    std::string szCommand = "open \"" + GetFileName() + "\" type mpegvideo alias " + GetFileName();		
    mciSendString(szCommand.c_str(), NULL, 0, 0);
  }

  std::string m_szFileName;
  bool m_bPaused;
};


int
main (int argc, char **argv)
{
  // CMP3_MCI	MyMP3;
  // MyMP3.Load((char *) "music.mp3");
  // MyMP3.Play();

  //getchar ();
  //return 0;

  // https://docs.microsoft.com/bg-bg/windows/desktop/Multimedia/mci-reference
  // https://docs.microsoft.com/bg-bg/windows/desktop/Multimedia/mci-functions
  // https://www.gamedev.net/forums/topic/401481-mci-with-c/

  
  messages = (Msg *) malloc (messages_max * sizeof (Msg));

  //connect_to (91, 92, 46, 28, 12345);  // libtec.org
  connect_to ("libtec.org");      //PhilipPC
  //connect_to (192, 168, 10, 110, 12345); //NOTHING
  
  if (SDL_Init (SDL_INIT_VIDEO) != 0)
    {
      puts ("ERROR!");
      return 1;
    }

  int chat_width = window_width / 4;
  int chat_height = window_height / 4;
  SDL_Window *window = SDL_CreateWindow ("Typtiza", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, SDL_WINDOW_OPENGL);
  assert (window);

  SDL_GL_CreateContext (window);

  FILE *load_file = fopen ("name.txt", "r");
  if (load_file)
    {
      fscanf (load_file, "%s", myname);
      fclose (load_file);
    }
  
  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor (0.1, 0.7, 0.4, 1.0);

  global_texture = load_image ("res/Typtiza.png");

  char text[4096];
  text[0] = 0;

  int current_dir_scroll = 0;
  int current_msg_scroll = 0;

  int keep_running = 1;
  while (keep_running)
    {
      SDL_Event event;
      while (SDL_PollEvent (&event))
        {
          if (event.type == SDL_MOUSEWHEEL)
            {
              if (file_entries_count)
                {
                  current_dir_scroll -= event.wheel.y;
                  if (current_dir_scroll < 0)
                    {
                      current_dir_scroll = 0;
                    }
                }
              else
                {
                  current_msg_scroll -= event.wheel.y;
                  if (current_msg_scroll < 0)
                    {
                      current_msg_scroll = 0;
                    }
                }
            }
          
          if (event.type == SDL_DROPFILE)
            {
              print ("DROPFILE: %s\n", event.drop.file);
              send_file (event.drop.file);
            }
          switch (event.type)
            {
            case SDL_MOUSEMOTION:
              {
                input_x = event.motion.x / 2 - chat_width;
                input_y = (window_height - event.motion.y) / 2 - chat_height;
              } break;
            case SDL_MOUSEBUTTONDOWN:
              {
                input_rmb = 1;
              } break;
            case SDL_MOUSEBUTTONUP:
              {
                input_rmb = 0;
              } break;
            }
              
          if      (event.type == SDL_DROPTEXT)     {print ("DROPTEXT\n");}
          else if (event.type == SDL_DROPBEGIN)    {print ("DROPBEGIN\n");}
          else if (event.type == SDL_DROPCOMPLETE) {print ("DROPCOMPLETE\n");}


          if (event.type == SDL_QUIT)
            {
              keep_running = 0;
            }
          if (event.type == SDL_TEXTINPUT)
            {
              strcat (text, event.text.text);
              fflush (stdout);
            }
          
          if (event.type == SDL_KEYDOWN)
            {
              if (event.key.keysym.sym == SDLK_UP)
                {
                  if (current_file > 0)
                    {
                      current_file -= 1;
                    }
                  else if (file_entries_count)
                    {
                      current_file = file_entries_count - 1;
                    }

                  if (current_file < current_dir_scroll)
                    {
                      current_dir_scroll = current_file;
                    }
                  else if (current_file - 22 > current_dir_scroll)
                    {
                      current_dir_scroll = current_file - 22;
                    }
                }
              if (event.key.keysym.sym == SDLK_DOWN)
                {
                  current_file += 1;
                  if (current_file >= file_entries_count)
                    {
                      current_file = 0;
                    }
                  
                  if (current_file < current_dir_scroll)
                    {
                      current_dir_scroll = current_file;
                    }
                  else if (current_file - 22 > current_dir_scroll)
                    {
                      current_dir_scroll = current_file - 22;
                    }
                }
              
              if (event.key.keysym.sym == SDLK_BACKSPACE)
                {
                  int len = strlen (text);
                  if (len > 0)
                    {
                      text[len - 1] = 0;
                    }
                }
              if (event.key.keysym.sym == SDLK_RETURN)
                {
                  if (file_entries_count)
                    {
                      open_current_file ();
                    }
                  else if (!strcmp (text, "-clear"))
                    {
                      // TODO: Delete allocated memory
                      messages_count = 0;
                    }
                  else if (!strcmp (text, "-close"))
                    {
                      keep_running = 0;
                    }
                  else if (!strcmp (text, "-send"))
                    {
                      list_dir ();
                    }
                  else if (!memcmp (text, "-send ", 6))
                    {
                      char *file_path = text + 6;
                      send_file (file_path);
                    }
                  else if (!memcmp (text, "-name ", 6))
                    {
                      char *name = text + 6;
                      strcpy (myname, name);
                      FILE* file = fopen ("name.txt", "w");
                      fprintf (file, "%s", myname);
                      fclose (file);
                    }
                  else
                    {
                      char buffer[strlen (text) + strlen (myname) + 2];
                      strcpy (buffer, myname);
                      strcat (buffer, ":");
                      strcat (buffer, text);
                      send (my_socket, buffer, strlen (buffer), 0);
                      assert (messages_count < messages_max);
                      Msg msg = make_message (text, NULL);
                      messages[messages_count++] = msg;
                    }
                  text[0] = 0;
                }
            }
          
          // if (event.type == SDL_TEXTEDITING)
          //   {
          //     printf ("edit: '%s' %d %d\n", event.edit.text, event.edit.start, event.edit.length);
          //     fflush (stdout);
          //   }
        }

      //text
      float matrix[16] = {
                          4.0f / window_width, 0.00, 0.00, 0.00,
                          0.00, 4.0f / window_height, 0.00, 0.00,
                          0.00, 0.00, 1.00, 0.00,
                          0.00, 0.00, 0.00, 1.00,
      };
      glLoadMatrixf (matrix);
      
      glClear (GL_COLOR_BUFFER_BIT);
      //----------------------box
      
      for (int i = 0; i < messages_count; ++i)
        {
          Msg msg = messages[i];
          float y = window_height /  4 - 10 - i * 18 + current_msg_scroll * 6;
          if (msg.name)
            {
              draw_left_message (msg, y);
            }
          else
            {
              draw_right_message (msg, y);
            }
        }

      //text --------------------------------
      if (file_entries_count)
        {
          int box_w = window_width / 4 - 30+7;
          int box_h = 10;
          int dir_y = window_height / 4 + current_dir_scroll * box_h - 2;
          int box_x = 30-7;
          int box_y = dir_y;
          int text_x = box_x + 7;

          if (input_x > box_x - box_w / 2 &&
              input_x < box_x + box_w / 2 &&
              input_y < box_y + box_h / 2)
            {
              int file_on = (box_y - input_y) / box_h;
              if (file_on < file_entries_count)
                {
                  current_file = file_on;
                  if (input_rmb)
                    {
                      input_rmb = 0;
                      open_current_file ();
                    }
                }
            }

          box_y -= current_file * box_h;
          
          glColor4f (0, 0.6, 0.3, 0.9);

          int bg_h = file_entries_count * box_h;
          draw_rect (box_x, window_height / 4, box_w, bg_h + 4);

          glColor3f (0.1, 0.832, 0.25);
          
          draw_rect (box_x, box_y, box_w, box_h);

          int y = dir_y - box_h;
          FOR (file_entries_count)
            //for (int i = current_file; i < current_file + 23; ++i)
            {
              int x = text_x;
              FileEntry entry = file_entries[i];
              if (entry.is_dir)
                {
                  draw_text (x - 6, y - 3, "*");
                }
              draw_text (x, y, entry.name);
              y -= box_h;
            }
        }

      //type space
      
      draw_type_space (68, -114.5);
      
      draw_text (-33 - 20, -118.5, text);

      //add button

      int add_button_x = -42 - 20;
      int add_button_y = -113;
      int add_button_w = 15;
      int add_button_h = 15;
      draw_add_button (add_button_x, add_button_y);

      //emojys

      draw_emojy_icon (-58 - 20, -113);

      //call button

      draw_call_button (-74 - 20, -113);

      //record button

      draw_record_button (-90 - 20, -113);

      //add button ----------------------------------------

      draw_rect (input_x, input_y, 5, 5); 
      
      if (input_rmb &&
          input_x > add_button_x - add_button_w / 2 &&
          input_x < add_button_x + add_button_w / 2 &&
          input_y > add_button_y - add_button_h / 2 &&
          input_y < add_button_y + add_button_h / 2)
        {
          input_rmb = 0;
          list_dir ();
        }
      
      //END
      SDL_GL_SwapWindow (window);
    }
  
  SDL_Quit();
  return 0;
}
