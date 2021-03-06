#ifndef TREE_EXCEPTION_H
#define TREE_EXCEPTION_H

#include <stdexcept>
#include <string>

namespace forest{
	class TreeException : public std::exception {
		
		public:
			enum class ERRORS{
				CANNOT_CREATE_ROOT,
				CANNOT_CREATE_FILE,
				NOT_VALID_TREE_TYPE,
				TREE_DOES_NOT_EXISTS,
				TREE_ALREADY_EXISTS,
				LEAF_DOES_NOT_EXISTS,
				CANNOT_READ_FILE,
				CANNOT_WRITE_FILE,
				FOREST_FOLDED,
				ACCESSING_END_LEAF,
				BAD_INPUT_PARAMETER
			};
			
			TreeException(ERRORS code)
			{
				msg = error_messages[(int)code];
			}
			const char * what () const throw ()
			{
				return msg.c_str();
			}
			
		private:
			std::string msg;
			static inline const std::string error_messages[11] = {
				"Cannot create root files",
				"Cannot create files",
				"Not valid tree type",
				"Trying to open not valid tree type",
				"Tree with name you trying to create is already exists",
				"Leaf does not exists in the tree",
				"Cannot read file",
				"Cannot write to file",
				"Forest Folded",
				"Trying to access End leaf data",
				"Inconvinient input parameter provided"
			};  
	};
}

#endif //TREE_EXCEPTION_H
