using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Events;
using UnityEngine.XR;

public class InputManager : MonoBehaviour
{
    public static InputManager instance;

    private InputFeatureUsage<Vector2> primary2DAxisInput = CommonUsages.primary2DAxis; //Joystick
    private InputFeatureUsage<float> triggerInput = CommonUsages.trigger; //Trigger
    private InputFeatureUsage<float> gripInput = CommonUsages.grip; //Grip
    private InputFeatureUsage<bool> primary2DAxisTouchInput = CommonUsages.primary2DAxisTouch; //Thumbstick touch
    private InputFeatureUsage<bool> primary2DAxisClickInput = CommonUsages.primary2DAxisClick; //Thumbstick press
    private InputFeatureUsage<bool> primaryButonInput = CommonUsages.primaryButton; //X/A press
    private InputFeatureUsage<bool> primaryTouchInput = CommonUsages.primaryTouch; //X/A touch
    private InputFeatureUsage<bool> secondaryButonInput = CommonUsages.secondaryButton; //X/A press
    private InputFeatureUsage<bool> secondaryTouchInput = CommonUsages.secondaryTouch; //X/A touch
    private InputFeatureUsage<bool> gripButtonInput = CommonUsages.gripButton; //Grip press
    private InputFeatureUsage<bool> triggerButtonInput = CommonUsages.triggerButton; //Trigger press
    private InputFeatureUsage<bool> menuButtonInput = CommonUsages.menuButton; //Menu press (left controller)
    private InputFeatureUsage<bool> userPresenceInput = CommonUsages.userPresence; //UserPresence - Use this property to test whether the user is currently wearing and/or interacting with the XR device. 

    public static InputManager Instance
    {
        get
        {
            return instance;
        }
    }

    void Awake()
    {
        if (instance != null && instance != this)
        {
            Destroy(this.gameObject);
        }
        instance = this;
    }

    // Start is called before the first frame update
    void Start()
    {
        
    }

    public InputFeatureUsage<Vector2> GetPrimary2DAxisInput()
    {
        return primary2DAxisInput;
    }

    public InputFeatureUsage<bool> GetPrimaryButton()
    {
        return primaryButonInput;
    }

    public InputFeatureUsage<bool> GetPrimary2DAxisClick()
    {
        return primary2DAxisClickInput;
    }


    //static readonly Dictionary<string, InputFeatureUsage<bool>> availableButtons = new Dictionary<string, InputFeatureUsage<bool>>
    //    {
    //        {"triggerButton", CommonUsages.triggerButton },
    //        {"thumbrest", CommonUsages.thumbrest },
    //        {"primary2DAxisClick", CommonUsages.primary2DAxisClick },
    //        {"primary2DAxisTouch", CommonUsages.primary2DAxisTouch },
    //        {"menuButton", CommonUsages.menuButton },
    //        {"gripButton", CommonUsages.gripButton },
    //        {"secondaryButton", CommonUsages.secondaryButton },
    //        {"secondaryTouch", CommonUsages.secondaryTouch },
    //        {"primaryButton", CommonUsages.primaryButton },
    //        {"primaryTouch", CommonUsages.primaryTouch },
    //    };

    //public enum ButtonOption
    //{
    //    triggerButton,
    //    thumbrest,
    //    primary2DAxisClick,
    //    primary2DAxisTouch,
    //    menuButton,
    //    gripButton,
    //    secondaryButton,
    //    secondaryTouch,
    //    primaryButton,
    //    primaryTouch
    //};

    //[Tooltip("Input device role (left or right controller)")]
    //public InputDeviceRole deviceRole;

    //[Tooltip("Select the button")]
    //public ButtonOption button;

    //[Tooltip("Event when the button starts being pressed")]
    //public UnityEvent OnPress;

    //[Tooltip("Event when the button is released")]
    //public UnityEvent OnRelease;

    //// to check whether it's being pressed
    //public bool IsPressed { get; private set; }

    //// to obtain input devices
    //List<InputDevice> inputDevices;
    //bool inputValue;

    //InputFeatureUsage<bool> inputFeature;

    //void Awake()
    //{
    //    // get label selected by the user
    //    string featureLabel = Enum.GetName(typeof(ButtonOption), button);

    //    // find dictionary entry
    //    availableButtons.TryGetValue(featureLabel, out inputFeature);

    //    // init list
    //    inputDevices = new List<InputDevice>();
    //}

    //void Update()
    //{
    //    InputDevices.GetDevicesWithRole(deviceRole, inputDevices);

    //    for (int i = 0; i < inputDevices.Count; i++)
    //    {
    //        if (inputDevices[i].TryGetFeatureValue(inputFeature,
    //            out inputValue) && inputValue)
    //        {
    //            // if start pressing, trigger event
    //            if (!IsPressed)
    //            {
    //                IsPressed = true;
    //                OnPress.Invoke();
    //            }
    //        }

    //        // check for button release
    //        else if (IsPressed)
    //        {
    //            IsPressed = false;
    //            OnRelease.Invoke();
    //        }
    //    }
    //}


}
