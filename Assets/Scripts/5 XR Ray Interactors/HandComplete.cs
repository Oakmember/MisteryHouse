using Shared;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.XR;
using static UnityEditor.Rendering.ShadowCascadeGUI;

public class HandComplete : MonoBehaviour
{
    [SerializeField]
    private HandType handType = HandType.None;

    [SerializeField]
    private HandStateType handStateType = HandStateType.None;

    //Stores handPrefab to be Instantiated
    public GameObject handPrefab;
    
    //Allows for hiding of hand prefab if set to true
    public bool hideHandOnSelect = false;
    
    //Stores what kind of characteristics we're looking for with our Input Device when we search for it later
    public InputDeviceCharacteristics inputDeviceCharacteristics;

    //Stores the InputDevice that we're Targeting once we find it in InitializeHand()
    private InputDevice _targetDevice;
    private Animator _handAnimator;
    private SkinnedMeshRenderer _handMesh;
    private bool isGrabbed = false;
    private PlayerManager playerManager = null;



   
    private void Start()
    {
        if (PlayerManager.Instance)
        {
            playerManager = PlayerManager.Instance;
        }

        InitializeHand();
    }

    private void InitializeHand()
    {
        List<InputDevice> devices = new List<InputDevice>();
        //Call InputDevices to see if it can find any devices with the characteristics we're looking for
        InputDevices.GetDevicesWithCharacteristics(inputDeviceCharacteristics, devices);

        //Our hands might not be active and so they will not be generated from the search.
        //We check if any devices are found here to avoid errors.
        if (devices.Count > 0)
        {
            
            _targetDevice = devices[0];

            //GameObject spawnedHand = Instantiate(handPrefab, transform);
            //_handAnimator = spawnedHand.GetComponent<Animator>();
            //_handMesh = spawnedHand.GetComponentInChildren<SkinnedMeshRenderer>();

            _handAnimator = handPrefab.GetComponent<Animator>();
            _handMesh = handPrefab.GetComponentInChildren<SkinnedMeshRenderer>();
        }
    }


    // Update is called once per frame
    private void Update()
    {
        //Since our target device might not register at the start of the scene, we continously check until one is found.
        if(!_targetDevice.isValid)
        {
            InitializeHand();
        }
        else
        {
            UpdateHand();
        }
    }

    private void UpdateHand()
    {
        //This will get the value for our trigger from the target device and output a flaot into triggerValue
        if (_targetDevice.TryGetFeatureValue(CommonUsages.trigger, out float triggerValue))
        {
            _handAnimator.SetFloat(Consts.trigger, triggerValue);
        }
        else
        {
            _handAnimator.SetFloat(Consts.trigger, 0);
        }
        //This will get the value for our grip from the target device and output a flaot into gripValue
        if (_targetDevice.TryGetFeatureValue(CommonUsages.grip, out float gripValue))
        {
            _handAnimator.SetFloat(Consts.grip, gripValue);
        }
        else
        {
            _handAnimator.SetFloat(Consts.grip, 0);
        }
    }

    public void HideHandOnSelect()
    {
        if (hideHandOnSelect)
        {
            _handMesh.enabled = !_handMesh.enabled;

           
        }

        isGrabbed = !isGrabbed;
        if (isGrabbed)
        {
            if (handType == HandType.Left)
            {
                playerManager.SetLeftHandState(HandStateType.Grab);
                handStateType = HandStateType.Grab;
            }
            else if (handType == HandType.Right)
            {
                playerManager.SetRightHandState(HandStateType.Grab);
                handStateType = HandStateType.Grab;
            }
        }
        else
        {
            if (handType == HandType.Left)
            {
                playerManager.SetLeftHandState(HandStateType.Drop);
                handStateType = HandStateType.Drop;
            }
            else if (handType == HandType.Right)
            {
                playerManager.SetRightHandState(HandStateType.Drop);
                handStateType = HandStateType.Drop;
            }
        }
    }

}
