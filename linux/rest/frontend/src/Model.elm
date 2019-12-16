module Model exposing (Model,Function,FunArg,init,argEncode,ArgType(..))
import Selectize exposing (State,entry,closed)
import Json.Encode exposing (Value,string,int,null,list,bool)
import Dict exposing (Dict,empty)
import List

-- Model
type alias Model =
    { selection : Maybe Function
    , menu : State Function
    -- this would be Dict FunArg String, but FunArg is not comparable, and elm doesn't have typeclasses
    -- key is the variable name, value is a tuple of current value and the FunArg description
    , setvars : Dict String (String,FunArg)
    , resp : String
    }

type alias Function =
    { name : String
    , args : (List FunArg)
    }


type ArgType
    = StringArg
    | IntArg
    | BoolArg
    | ListArg ArgType

-- A funarg has a name, and a method to encode it into a JSON object
-- the encoder is currently unused, so arguments are really just strings
type alias FunArg =
    { name : String
    , argType : ArgType
    , optional : Bool
    }

argEncode : ArgType -> String -> Maybe Value
argEncode a s = case a of
    StringArg -> string s |> Just
    IntArg -> String.toInt s |> Maybe.map int
    BoolArg -> case String.toLower s of
        "true" -> bool True |> Just
        "false" -> bool True |> Just
        _ -> Nothing
    ListArg t2 ->
        let
            strlist : List String
            strlist = List.map String.trim <| String.split "," s
            vallist : List Value
            vallist = List.filterMap (argEncode t2) strlist
        in Just <| null




strArg : String -> FunArg
strArg s = FunArg s StringArg False
optStrArg s = FunArg s StringArg True


-- some commonly used arguments. Others are defined inline
userToken = strArg "userToken"
applicationID = strArg "applicationID"
roleID = strArg "roleID"
username = strArg "username"
password = strArg "password"
virtueID = strArg "virtueID"


appCreate = Function "application create"
    [ userToken
    , applicationID
    , optStrArg "launchCmd"
    , optStrArg "install"
    , optStrArg "icon"
    ]
appGet = Function "application get" [userToken,applicationID]
appList = Function "application list" [userToken]

roleGet = Function "role get" [userToken,roleID]
roleCreate = Function "role create"
    [ userToken
    , roleID
    , strArg "repoID"
    , FunArg "appIDs" (ListArg StringArg) False
    ]
roleList = Function "role list" [userToken]

userLogin = Function "user login" [username,password]
userLogout = Function "user logout" [userToken]
userCreate = Function "user create" 
    [ userToken
    , username
    , password
    , FunArg "isAdmin" BoolArg True
    ]
userRoleList = Function "user role list" [userToken]
userRoleAuthorize = Function "user role authorize" [userToken,username,roleID]
userList = Function "user list" [userToken]
userGet = Function "user get" [userToken,username]

virtueList = Function "virtue list" 
    [ userToken
    , optStrArg "userID"
    , optStrArg "roleID"
    , optStrArg "virtueID"
    , FunArg "mine" BoolArg True
    ]
virtueGet = Function "virtue get" [userToken,virtueID]
virtueCreate = Function "virtue create" [userToken,roleID]
virtueDestroy = Function "virtue destroy" [userToken,virtueID]
-- virtue launch, virtue stop not implemented


funList =
    [ appCreate
    , appGet
    , appList
    , roleGet
    , roleCreate
    , roleList
    , userLogin
    , userLogout
    , userCreate
    , userRoleList
    , userRoleAuthorize
    , userList
    , virtueList
    ]

funView : Function -> String
funView f = f.name

init : () -> (Model, Cmd msg)
init _ =
    (
        { selection = Nothing
        , menu = closed "unique-menu" funView (List.map Selectize.entry funList)
        , setvars = empty
        , resp = ""
        }
    ,Cmd.none
    )
